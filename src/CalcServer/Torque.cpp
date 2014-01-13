/*
 *  This file is part of AQUAgpusph, a free CFD program based on SPH.
 *  Copyright (C) 2012  Jose Luis Cercos Pita <jl.cercos@upm.es>
 *
 *  AQUAgpusph is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AQUAgpusph is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with AQUAgpusph.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ProblemSetup.h>
#include <ScreenManager.h>
#include <CalcServer/Torque.h>
#include <CalcServer.h>

namespace Aqua{ namespace CalcServer{

Torque::Torque()
	: Kernel("Torque")
	, _device_torque(NULL)
	, _device_force(NULL)
	, _program(NULL)
	, _path(NULL)
	, _kernel(NULL)
	, _global_work_size(0)
	, _local_work_size(0)
	, _torque_reduction(NULL)
	, _force_reduction(NULL)
{
	InputOutput::ScreenManager *S = InputOutput::ScreenManager::singleton();
	InputOutput::ProblemSetup *P = InputOutput::ProblemSetup::singleton();
	unsigned int str_len = strlen(P->OpenCL_kernels.torque);
	if(str_len <= 0) {
		S->addMessageF(3, "The path of the kernel is empty.\n");
		exit(EXIT_FAILURE);
	}
	_path = new char[str_len+4];
	if(!_path) {
		S->addMessageF(3, "Memory cannot be allocated for the path.\n");
		exit(EXIT_FAILURE);
	}
	strcpy(_path, P->OpenCL_kernels.torque);
	strcat(_path, ".cl");

	_local_work_size  = localWorkSize();
	if(!_local_work_size){
	    S->addMessageF(3, "I cannot get a valid local work size for the required computation tool.\n");
	    exit(EXIT_FAILURE);
	}
	_global_work_size = globalWorkSize(_local_work_size);
	if(setupTorque()) {
		exit(EXIT_FAILURE);
	}
	if(setupReduction()) {
		exit(EXIT_FAILURE);
	}
	_torque.x = 0.f;
	_torque.y = 0.f;
	_force.x  = 0.f;
	_force.y  = 0.f;
	#ifdef HAVE_3D
		_torque.z = 0.f;
		_force.z  = 0.f;
	#endif
	S->addMessageF(1, "Torque ready to work!\n");
}

Torque::~Torque()
{
	InputOutput::ScreenManager *S = InputOutput::ScreenManager::singleton();
	S->addMessageF(1, "Destroying torque reduction processor...\n");
	if(_torque_reduction) delete _torque_reduction;
	S->addMessageF(1, "Destroying force reduction processor...\n");
	if(_force_reduction)  delete _force_reduction;
	if(_device_torque)clReleaseMemObject(_device_torque); _device_torque=0;
	if(_device_force)clReleaseMemObject(_device_force); _device_force=0;
	if(_kernel)clReleaseKernel(_kernel); _kernel=0;
	if(_program)clReleaseProgram(_program); _program=0;
	if(_path)delete[] _path; _path=0;
}

bool Torque::execute()
{
	InputOutput::ScreenManager *S = InputOutput::ScreenManager::singleton();
	CalcServer *C = CalcServer::singleton();
	unsigned int i;
	int err_code=0;

	err_code |= sendArgument(_kernel,
                             10,
                             sizeof(vec),
                             (void*)&(C->g));
	err_code |= sendArgument(_kernel,
                             11,
                             sizeof(vec),
                             (void*)&_cor);
	if(err_code != CL_SUCCESS) {
		S->addMessageF(3, "I cannot send a variable to the kernel.\n");
		return true;
	}
	#ifdef HAVE_GPUPROFILE
		cl_event event;
		cl_ulong end, start;
		profileTime(0.f);
		err_code = clEnqueueNDRangeKernel(C->command_queue,
                                          _kernel,
                                          1,
                                          NULL,
                                          &_global_work_size,
                                          NULL,
                                          0,
                                          NULL,
                                          &event);
	#else
		err_code = clEnqueueNDRangeKernel(C->command_queue,
                                          _kernel,
                                          1,
                                          NULL,
                                          &_global_work_size,
                                          NULL,
                                          0,
                                          NULL,
                                          NULL);
	#endif
	if(err_code != CL_SUCCESS) {
		S->addMessageF(3, "I cannot execute torque calculation kernel.\n");
        S->printOpenCLError(err_code);
	    return true;
	}
	#ifdef HAVE_GPUPROFILE
	    err_code = clWaitForEvents(1, &event);
	    if(err_code != CL_SUCCESS) {
	        S->addMessage(3, "Impossible to wait for the kernels end.\n");
            S->printOpenCLError(err_code);
	        return true;
	    }
	    err_code |= clGetEventProfilingInfo(event,
                                            CL_PROFILING_COMMAND_END,
                                            sizeof(cl_ulong),
                                            &end,
                                            0);
	    if(err_code != CL_SUCCESS) {
	        S->addMessage(3, "I cannot profile the kernel execution.\n");
            S->printOpenCLError(err_code);
	        return true;
	    }
	    err_code |= clGetEventProfilingInfo(event,
                                            CL_PROFILING_COMMAND_START,
                                            sizeof(cl_ulong),
                                            &start,
                                            0);
	    if(err_code != CL_SUCCESS) {
	        S->addMessage(3, "I cannot profile the kernel execution.\n");
            S->printOpenCLError(err_code);
	        return true;
	    }
		profileTime(profileTime() + (end - start)/1000.f);  // 10^-3 ms
	#endif
    cl_mem output;
    output = _torque_reduction->execute();
    if(!output)
        return true;
	if(C->getData((void *)&_torque, output, sizeof(vec)))
		return true;
    output = _force_reduction->execute();
    if(!output)
        return true;
	if(C->getData((void *)&_force, output, sizeof(vec)))
		return true;
	return false;
}

bool Torque::setupTorque()
{
	InputOutput::ScreenManager *S = InputOutput::ScreenManager::singleton();
	CalcServer *C = CalcServer::singleton();
	char msg[1024];
	cl_int err_code=0;
	if(!loadKernelFromFile(&_kernel,
                           &_program,
                           C->context,
                           C->device,
                           _path,
                           "Torque",
                           ""))
		return true;
	_device_torque = C->allocMemory(C->n * sizeof( vec ));
	_device_force = C->allocMemory(C->n * sizeof( vec ));
	if(!_device_torque || !_device_force)
		return true;
	sprintf(msg, "\tAllocated memory = %u bytes\n", (unsigned int)C->allocated_mem);
	S->addMessage(0, msg);
	err_code  = sendArgument(_kernel,
                             0,
                             sizeof(cl_mem),
                             (void*)&_device_torque);
	err_code |= sendArgument(_kernel,
                             1,
                             sizeof(cl_mem),
                             (void*)&_device_force);
	err_code |= sendArgument(_kernel,
                             2,
                             sizeof(cl_mem),
                             (void*)&(C->imove));
	err_code |= sendArgument(_kernel,
                             3,
                             sizeof(cl_mem),
                             (void*)&(C->ifluid));
	err_code |= sendArgument(_kernel,
                             4,
                             sizeof(cl_mem),
                             (void*)&(C->pos));
	err_code |= sendArgument(_kernel,
                             5,
                             sizeof(cl_mem),
                             (void*)&(C->f));
	err_code |= sendArgument(_kernel,
                             6,
                             sizeof(cl_mem),
                             (void*)&(C->mass));
	err_code |= sendArgument(_kernel,
                             7,
                             sizeof(cl_mem),
                             (void*)&(C->dens));
	err_code |= sendArgument(_kernel,
                             8,
                             sizeof(cl_mem),
                             (void*)&(C->refd));
	err_code |= sendArgument(_kernel,
                             9,
                             sizeof(cl_uint),
                             (void*)&(C->n));
	err_code |= sendArgument(_kernel,
                             10,
                             sizeof(vec),
                             (void*)&(C->g));
	if(err_code)
		return true;

	cl_device_id device;
	size_t local_work_size=0;
	err_code |= clGetCommandQueueInfo(C->command_queue,CL_QUEUE_DEVICE,
	                                sizeof(cl_device_id),&device, NULL);
	if(err_code != CL_SUCCESS) {
		S->addMessageF(3, "I Cannot get the device from the command queue.\n");
        S->printOpenCLError(err_code);
        return true;
	}
	err_code |= clGetKernelWorkGroupInfo(_kernel,device,CL_KERNEL_WORK_GROUP_SIZE,
	                                   sizeof(size_t), &local_work_size, NULL);
	if(err_code != CL_SUCCESS) {
		S->addMessageF(3, "Failure retrieving the maximum local work size.\n");
        S->printOpenCLError(err_code);
	    return true;
	}
	if(local_work_size < _local_work_size)
	    _local_work_size  = local_work_size;
	_global_work_size = globalWorkSize(_local_work_size);
	return false;
}

bool Torque::setupReduction()
{
	CalcServer *C = CalcServer::singleton();
	#ifdef HAVE_3D
        _torque_reduction = new Reduction(_device_torque,
                                          C->n,
                                          "vec",
                                          "(vec)(0.f,0.f,0.f,0.f)", "c = a + b;");
        _force_reduction = new Reduction(_device_force,
                                         C->n,
                                         "vec",
                                         "(vec)(0.f,0.f,0.f,0.f)", "c = a + b;");
    #else
        _torque_reduction = new Reduction(_device_torque,
                                          C->n,
                                          "vec",
                                          "(vec)(0.f,0.f)", "c = a + b;");
        _force_reduction = new Reduction(_device_force,
                                         C->n,
                                         "vec",
                                         "(vec)(0.f,0.f)", "c = a + b;");
    #endif
	return false;
}

}}  // namespace

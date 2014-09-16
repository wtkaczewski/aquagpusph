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

/** @file
 * @brief Aqua::CalcServer tools base class.
 * (See Aqua::CalcServer::Kernel for details)
 */

#ifndef KERNEL_H_INCLUDED
#define KERNEL_H_INCLUDED

#include <sphPrerequisites.h>

#include <deque>
#include <CL/cl.h>
#include <clang-c/Index.h>
#include <clang-c/Platform.h>

#include <CalcServer/Tool.h>
#include <AuxiliarMethods.h>

namespace Aqua{ namespace CalcServer{

/** @class Kernel Kernel.h CalcServer/Kernel.h
 * @brief Base class for all the tools of the calculation server
 * Aqua::CalcServer.
 */
class Kernel : public Aqua::CalcServer::Tool
{
public:
    /** @brief Constructor.
     * @param tool_name Tool name.
     * @param kernel_path Kernel path.
     * The tool name will be used later to refer to the results of the tool.
     */
    Kernel(const char* tool_name, const char* kernel_path);

    /// Destructor
    virtual ~Kernel();

    /** @brief Initialize the tool.
     * @return false if all gone right, true otherwise.
     */
    bool setup();

    /** @brief Execute the tool.
     * @return false if all gone right, true otherwise.
     */
    bool execute();

    /** @brief Set the kernel file path.
     * @param kernel_path kernel file path.
     */
    void path(const char* kernel_path);

    /** @brief Get the kernel file path.
     * @return Tool kernel file path.
     */
    const char* path(){return (const char*)_path;}

    /** @brief Local work size for the selected device and tool.
     * @return Local work size.
     */
    size_t workGroupSize() const {return _work_group_size;}

    /** @brief Global work size.
     *
     * The global work size is the total number of threads needed to perform
     * the work.
     *
     * @return Global Work size.
     * @see workGroupSize()
     */
    size_t globalWorkSize() const {return _global_work_size;}

protected:
    /** @brief Compile the OpenCL program.
     * @param entry_point Program entry point function.
     * @param flags Compiler additional flags.
     * @param header Header to be append at the start of the source code.
     * @return false if all gone right, true otherwise.
     */
    bool compile(const char* entry_point="main",
                 const char* flags="",
                 const char* header="");

    /** @brief Compute the variables required by the program
     * @param entry_point Program entry point method.
     * @return false if all gone right, true otherwise.
     */
    bool variables(const char* entry_point="main");

    /** @brief Set the variables to the OpenCL kernel.
     *
     * The method detects if a variable should be updated or if it already set
     * either.
     * @return false if all gone right, true otherwise.
     */
    bool setVariables();

    /** @brief Compute the global work size.
     * @return false if all gone right, true otherwise.
     * @see globalWorkSize()
     */
    bool computeGlobalWorkSize();

private:
    /// Kernel path
    char* _path;

    /// OpenCL kernel
    cl_kernel _kernel;

    /// work group size
    size_t _work_group_size;

    /// List of required variables
    std::deque<char*> _var_names;
    /// List of variable sizes
    std::deque<size_t> _var_sizes;
    /// List of variable values
    std::deque<void*> _var_values;
};

}}  // namespace

#endif // KERNEL_H_INCLUDED

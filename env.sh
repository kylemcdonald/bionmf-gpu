
##########################################################################
 #
 # NMF-mGPU -- Non-negative Matrix Factorization on multi-GPU systems.
 #
 # Copyright (C) 2011-2014:
 #
 #	Edgardo Mejia-Roa(*), Carlos Garcia(*), Jose Ignacio Gomez(*),
 #	Manuel Prieto(*), Francisco Tirado(*) and Alberto Pascual-Montano(**).
 #
 #	(*)  ArTeCS Group, Complutense University of Madrid (UCM), Spain.
 #	(**) Functional Bioinformatics Group, Biocomputing Unit,
 #		National Center for Biotechnology-CSIC, Madrid, Spain.
 #
 #	E-mail for E. Mejia-Roa: <edgardomejia@fis.ucm.es>
 #	E-mail for A. Pascual-Montano: <pascual@cnb.csic.es>
 #
 #
 # This file is part of NMF-mGPU.
 #
 # NMF-mGPU is free software: you can redistribute it and/or modify
 # it under the terms of the GNU General Public License as published by
 # the Free Software Foundation, either version 3 of the License, or
 # (at your option) any later version.
 #
 # NMF-mGPU is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 # GNU General Public License for more details.
 #
 # You should have received a copy of the GNU General Public License
 # along with NMF-mGPU. If not, see <http://www.gnu.org/licenses/>.
 #
##########################################################################

##########################################################################
 #
 # Sets the required environment to (compile and) execute NMF-mGPU.
 #
 # Usage:
 #	.  ./env.sh  [ <path_to_CUDA_Toolkit> ]
 #
 # If the argument is not specified, it will be derived by looking for
 # <nvcc_basename> in all folders stored in your "PATH" environment variable.
 #
 # On some shells (e.g., dash), the argument must be specified as follow:
 #	set -- <path_to_CUDA_Toolkit> ; . ./env.sh
 #
 # WARNING:
 #	* Folder names containing whitespace characters are NOT supported. In
 #	  that case, either use a (soft) link, or rename your CUDA installation
 #	  directory.
 #
 #	* This script may not work on (t)csh. Please use another shell, such as
 #	  "bash", "dash", "ksh" or "zsh".
 #
##############################

# Default CUDA Compiler.
nvcc_basename="nvcc"


#############################
# Retrieves CUDA_HOME
#############################

# Argument was specified:
if [ -n "$1" ]  ; then

	CUDA_HOME="$1"

	if [ ! -d "${CUDA_HOME}" ] ; then
		echo "'${CUDA_HOME}' not found. Please provide an existing path to your CUDA Toolkit." >&2
		unset CUDA_HOME nvcc_basename
		return 1
	fi

else
	# No argument was specified. Searches for <nvcc_basename> in all folders stored in PATH.

	nvcc_path="$(which ${nvcc_basename})"

	if [ -z "${nvcc_path}" ] ; then
		echo "
Error: no argument was specified, and could not find any path to '${nvcc_basename}' in
your \"PATH\" environment variable. Please, either provide the path to your CUDA
Toolkit, or add to \"PATH\" the path to '${nvcc_basename}'.

Please note that in some shells (e.g., dash), arguments must be set as follow:
	set -- <path_to_CUDA_Toolkit> ; . ./env.sh
" >&2
		unset nvcc_path nvcc_basename
		return 1
	fi

	# Path found:
	CUDA_HOME="${nvcc_path%/*/${nvcc_basename}*}"
	unset nvcc_path
fi
unset nvcc_basename


# Checks for whitespace characters in folder name.
echo "${CUDA_HOME}" | grep ' ' > /dev/null 2>&1
if [ "$?" -eq 0 ] ; then
	echo "
Error: '${CUDA_HOME}'
Folder names containing whitespace characters are NOT supported. Please,
either use a (soft) link, or rename your CUDA installation directory.
" >&2
	unset CUDA_HOME
	return 1
fi

# Removes any leading slash ('/') character
CUDA_HOME="${CUDA_HOME%/}"


echo "
Setting CUDA_HOME to '${CUDA_HOME}'
"
export CUDA_HOME


#############################
# Updates PATH
#############################

cuda_bin="${CUDA_HOME}/bin"

if [ -d "${cuda_bin}" ] ; then
	if [ "${PATH}" = "${PATH#*${cuda_bin}}" ] ; then
		export PATH="${cuda_bin}":${PATH}
	fi
else
	echo "Warning: '${cuda_bin}' not found, so not added to PATH." >&2
fi

unset cuda_bin

#############################
# Updates (DY)LD_LIBRARY_PATH
#############################

# Path to CUDA library.
cuda_lib="${CUDA_HOME}/lib"

if [ ! -d "${cuda_lib}" ] ; then

	# 32- or 64-bits platform
	os_size=$(uname -m | sed -e "s/i.86/32/" -e "s/x86_64/64/" -e "s/armv7l/32/")

	if [ ! -d "${cuda_lib}${os_size}" ] ; then	# e.g., <CUDA_HOME>/lib64
		echo "Warning: Neither of '${cuda_lib}' or '${cuda_lib}${os_size}' were found, so not added to (DY)LD_LIBRARY_PATH." >&2
		unset os_size cuda_lib
		return 1
	fi

	cuda_lib+="${os_size}"
	unset os_size
fi

if [ "${LD_LIBRARY_PATH}" = "${LD_LIBRARY_PATH#*${cuda_lib}}" ] ; then
	export LD_LIBRARY_PATH="${cuda_lib}":${LD_LIBRARY_PATH}
fi

if [ "${DYLD_LIBRARY_PATH}" = "${DYLD_LIBRARY_PATH#*${cuda_lib}}" ] ; then
	export DYLD_LIBRARY_PATH="${cuda_lib}":${DYLD_LIBRARY_PATH}
fi

unset cuda_lib

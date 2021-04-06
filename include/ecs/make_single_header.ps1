# Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

# Set output to utf-8
$PSDefaultParameterValues['Out-File:Encoding'] = 'utf8'

# Get all the needed include files from the ecs_sh_incudes text file
$files = (type ecs_sh_includes.txt) -replace '^.*?"(.*?)".*?$','$1'

# Remove empty lines
$files = $files.trim() -ne ""

# Filter out the local includes from the content of each header and pipe it to ecs_sh.h
(sls -Path $files -SimpleMatch -Pattern '#include "' -NotMatch).Line > ecs_sh.h

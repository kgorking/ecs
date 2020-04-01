# Get all the local include files from the ecs.h header
$files = (type ecs.h) -replace '^.*?"(.*?)".*?$','$1'

# Remove empty lines
$files = $files.trim() -ne ""

# Filter out the local includes from the content of each header and pipe it to ecs_sh.h
(sls -Path $files -SimpleMatch -Pattern '#include "' -NotMatch).Line > ecs_sh.h

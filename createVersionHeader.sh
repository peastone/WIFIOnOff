#!/bin/bash

VERSION=`git describe --broken`
cat > src/wifionoff_version.h <<EOL
#ifndef WIFIONOFF_VERSION_H
#define WIFIONOFF_VERSION_H
/**
   @brief Version number - obtained by 'git describe'
*/
#define VERSION_BY_GIT_DESCRIBE "${VERSION}"
#endif
EOL

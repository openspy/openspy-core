RUN_FILE_BASEDIR=/opt/openspy/bin
WORKDIR=/opt/openspy
FILE_TO_RUN=$RUN_FILE_BASEDIR/$RUN_FILE
eval cd $WORKDIR
eval bash -c $FILE_TO_RUN
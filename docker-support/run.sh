RUN_FILE_BASEDIR=/opt/openspy/bin
WORKDIR=/opt/openspy
FILE_TO_RUN=$RUN_FILE_BASEDIR/$RUN_FILE
eval cd $WORKDIR

#this stuff is for FESL/SSL, need to write from env var due to docker image
if [[ -n $FESL_TOS_DATA ]]; then
    echo $FESL_TOS_DATA | base64 -d > fesl_tos.txt
    export OPENSPY_FESL_TOS_PATH=fesl_tos.txt
fi
if [[ -n $SSL_CERT_DATA ]]; then
    echo $SSL_CERT_DATA | base64 -d > ssl_cert.pem
    export OPENSSP_SSL_CERT=ssl_cert.pem
fi

if [[ -n $SSL_KEY_DATA ]]; then
    echo $SSL_KEY_DATA | base64 -d > ssl_key.pem
    export OPENSPY_SSL_KEY=ssl_key.pem
fi

if [[ -n $UTMASTER_MOTD_DATA ]]; then
    echo $UTMASTER_MOTD_DATA | base64 -d > motd.txt
fi

if [[ -n $UTMASTER_MOTD_COMMUNITY_DATA ]]; then
    echo $UTMASTER_MOTD_COMMUNITY_DATA | base64 -d > motd_community.txt
fi

eval bash -c $FILE_TO_RUN
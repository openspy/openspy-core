# our base image
FROM python:3-onbuild

ENV SQL_CONN_STR=mysql://openspy:openspy123@openspy.cbq9edz8fbuw.us-east-1.rds.amazonaws.com/GameTracker

ENV REDIS_SERV=openspy.u95v0m.0001.use1.cache.amazonaws.com
ENV REDIS_PORT=6379

EXPOSE 8000

RUN apt-get install libssl-dev

ADD authservices /tmp/authservices
ADD start_authservices.sh /tmp/start_authservices.sh
RUN chmod a+x /tmp/start_authservices.sh
ADD requirements.txt /tmp/requirements.txt

RUN pip install -r /tmp/requirements.txt

# Run it
ENTRYPOINT ["/tmp/start_authservices.sh"]

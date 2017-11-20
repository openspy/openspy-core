# our base image
FROM python:3-onbuild

ENV SQL_CONN_STR=mysql://openspy:openspy123@openspy.cbq9edz8fbuw.us-east-1.rds.amazonaws.com/GameTracker

ENV REDIS_SERV=opensly-redis.u95v0m.0001.use1.cache.amazonaws.com
ENV REDIS_PORT=6379

EXPOSE 8000

RUN apt-get install libssl-dev

RUN mkdir /web
ADD authservices /web/authservices
ADD start_authservices.sh /web/start_authservices.sh
RUN chmod a+x /web/start_authservices.sh
ADD requirements.txt /web/requirements.txt

RUN pip install -r /web/requirements.txt

# Run it
ENTRYPOINT ["python", "/web/authservices/Backend_Services.py"]

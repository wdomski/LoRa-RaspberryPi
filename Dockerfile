FROM python:3.7

# setup timezone
ENV TZ=Europe/Warsaw

# install additional packages and clean redundant files
RUN apt update && apt dist-upgrade -y && \
    apt install -y iputils-ping net-tools less wget vim && \
    rm -rf /var/lib/apt/lists/*

# upgrade pip to specific version
RUN pip install --upgrade pip==22.3.1

# install wiringpi dependancy
RUN wget https://github.com/WiringPi/WiringPi/releases/download/2.61-1/wiringpi-2.61-1-armhf.deb
RUN dpkg -i wiringpi-2.61-1-armhf.deb

# create app main directory
RUN mkdir /app

# copy content of the directory to /app, this includes lora.c and Makefile
COPY . /app
WORKDIR /app

# compile the LoRa lib
RUN make -f Makefile_docker clean && make -f Makefile_docker all

# execute a Bash script; the script should run indefinitely or instead you can
# start a service which will keep the docer running
CMD /app/start.sh 
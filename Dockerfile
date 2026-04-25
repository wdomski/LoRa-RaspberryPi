FROM python:3.13

# setup timezone
ENV TZ=Europe/Warsaw

# install additional packages and clean redundant files
RUN apt update && apt dist-upgrade -y && \
    apt install -y iputils-ping net-tools less wget vim && \
    rm -rf /var/lib/apt/lists/*

# upgrade pip to specific version
RUN pip install --upgrade pip==25.1.1

# install wiringpi dependancy
RUN wget https://github.com/WiringPi/WiringPi/releases/download/3.16/wiringpi_3.16_arm64.deb
RUN dpkg -i wiringpi_3.16_arm64.deb

# create app main directory
RUN mkdir /app

# copy content of the directory to /app, this includes lora.c and Makefile
COPY . /app
WORKDIR /app

# compile the LoRa lib
RUN make clean && make lib TARGET=DOCKER

# execute a Bash script; the script should run indefinitely or instead you can
# start a service which will keep the docer running
RUN chmod +x /app/start.sh
CMD /app/start.sh 
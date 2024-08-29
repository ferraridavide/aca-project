FROM ubuntu

RUN apt-get update && apt-get upgrade -y
RUN apt-get install -y openssh-server wget git nano curl iputils-ping

RUN apt install -y build-essential
RUN apt-get install -y openmpi-bin openmpi-doc libopenmpi-dev

RUN useradd -m -s /bin/bash mpi \
    && echo 'mpi:mpi' | chpasswd

# Allow SSH access for the non-root user
RUN mkdir -p /home/mpi/.ssh \
    && chmod 700 /home/mpi/.ssh \
    && touch /home/mpi/.ssh/authorized_keys \
    && chmod 600 /home/mpi/.ssh/authorized_keys \
    && chown -R mpi:mpi /home/mpi/.ssh

RUN mkdir /var/run/sshd \
    && mkdir /var/run/sshd/empty

RUN apt-get install sudo
RUN usermod -aG sudo mpi

EXPOSE 22

CMD ["/usr/sbin/sshd", "-D"]
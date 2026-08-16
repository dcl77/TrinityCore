<<<<<<< HEAD
FROM trinitycore/circle-ci:debian-12-runner
=======
FROM trinitycore/circle-ci:debian-13-runner
>>>>>>> upstream/3.3.5

#Setup
WORKDIR /home/circleci
COPY README.md /
COPY --chown=circleci:circleci sql /home/circleci/project/sql
COPY bin /usr/local/bin/
ENTRYPOINT [ "echo",  "Check the README.md file for instructions"]

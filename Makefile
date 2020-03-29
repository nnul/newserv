
all: build

.PHONY: build
build: libphosg
	@cd src && make build

.PHONY: libphosg
libphosg:
	@cd external/phosg && make build && make install
	@cd src/ && ln -s ../external/phosg/libphosg

.PHONY: clean
clean:
	@cd external/phosg && make clean
	@cd src/ && rm -rf libphosg
	@cd src && find . -name \*.o -delete
	@cd src && rm -rf *.dSYM newserv newserv-dns gmon.out

.PHONY: docker-build
docker-build:
	#docker build -t pso-newserv:latest .
	docker-compose -p pso-newserv build

.PHONY: docker-start
docker-start:
	docker-compose -p pso-newserv up -d

.PHONY: docker-stop
docker-stop:
	docker-compose -p pso-newserv stop

.PHONY: docker-run
docker-logs:
	docker-compose -p pso-newserv logs --tail 100 -f




USERDEFS = include/user_ta_header_defines.h

.PHONY: client clean all 


all: client out
	cp client/out/sece_nv_ftpm_eval out

names.mk:
	make init

client: $(USERDEFS) names.mk | out
	cd client && make

out:
	mkdir -p out

clean:
	rm -rf out
	cd client && make clean





all:
ifeq ($(VSPM_LEGACY_IF),1)
	${CC} -fPIC -c vspm_api.c vspm_api_vsp.c -I../include -I$(INCSHARED) -Wall
	${CC} -shared -Wl,-soname=libvspm.so.1 -o libvspm.so.1.0.0 vspm_api.o vspm_api_vsp.o
else
	${CC} -fPIC -c vspm_api.c -I../include -I$(INCSHARED) -Wall
	${CC} -shared -Wl,-soname=libvspm.so.1 -o libvspm.so.1.0.0 vspm_api.o
endif
	ln -s ./libvspm.so.1.0.0 libvspm.so.1
	ln -s ./libvspm.so.1 libvspm.so
clean:
	rm -f vspm_api.o
	rm -f vspm_api_vsp.o
	rm -f libvspm.so
	rm -f libvspm.so.1.0.0
	rm -f libvspm.so.1
install:
	$(CP) ../include/vspm_public.h $(INCSHARED)


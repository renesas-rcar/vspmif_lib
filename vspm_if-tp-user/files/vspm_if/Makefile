all:
	${CC} vspm_tp_uds.c \
	-I$(INCSHARED) \
	-L$(LIBSHARED) \
	-lvspm -lpthread \
	-o vspm_tp_uds

	${CC} vspm_tp_colorkey.c \
	-I$(INCSHARED) \
	-L$(LIBSHARED) \
	-lvspm -lpthread \
	-o vspm_tp_colorkey

clean:
	rm -f vspm_tp_uds
	rm -f vspm_tp_colorkey


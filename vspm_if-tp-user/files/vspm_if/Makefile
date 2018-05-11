all:
	${CC} vspm_tp_lossy.c \
	-I$(INCSHARED) \
	-L$(LIBSHARED) \
	-lvspm -lmmngr -lpthread \
	-o vspm_tp

	${CC} fdpm_tp.c \
	-I$(INCSHARED) \
	-L$(LIBSHARED) \
	-lvspm -lmmngr -lpthread \
	-o fdpm_tp

	${CC} vspm_tp_uds.c \
	-I$(INCSHARED) \
	-L$(LIBSHARED) \
	-lvspm -lmmngr -lpthread \
	-o vspm_tp_uds

	${CC} vspm_tp_colorkey.c \
	-I$(INCSHARED) \
	-L$(LIBSHARED) \
	-lvspm -lmmngr -lpthread \
	-o vspm_tp_colorkey

clean:
	rm -f vspm_tp
	rm -f fdpm_tp
	rm -f vspm_tp_uds
	rm -f vspm_tp_colorkey


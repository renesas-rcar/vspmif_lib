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

clean:
	rm -f vspm_tp
	rm -f fdpm_tp


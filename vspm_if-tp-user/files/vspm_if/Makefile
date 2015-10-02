all:
	${CC} vspm_tp.c \
	-I$(BUILDDIR)/include \
	-L$(LIBSHARED) \
	-lvspm -lmmngr -lpthread \
	-o vspm_tp

	${CC} fdpm_tp.c \
	-I$(BUILDDIR)/include \
	-L$(LIBSHARED) \
	-lvspm -lmmngr -lpthread \
	-o fdpm_tp

clean:
	rm -f vspm_tp
	rm -f fdpm_tp


MODULE_big = template_fdw
OBJS = template_fdw.o
DATA = template_fdw--0.0.sql
EXTENSION = template_fdw

REGRESS_OPTS = --dbname=$(PL_TESTDB)
REGRESS = template_fdw

ifdef NO_PGXS
subdir = contrib/template_fdw
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
else
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
endif

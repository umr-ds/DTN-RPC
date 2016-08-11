all:
	$(MAKE) -C src/

clean:
	$(RM) -rf autom4te.cache/ aclocal.m4 config.log config.status servalrpc

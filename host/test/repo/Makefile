target = repo
CC = clang
incl = -I../../../kernel/include -I../../../kernel/interface -I../../../kernel/arch/x86/include -I../../../include -I../../../interface
cflags = -Os -g -c -Wall -ansi $(incl) -D_CORE_ -D_x86_64_  -D_64_
lflags = -Wl,-T../../common/vx.ld
flist = files
flist := $(shell cat $(flist))
csrc := $(flist)
cobj := $(csrc:.c=.o)
cdep := $(csrc:.c=.d)
fnames := $(basename $(csrc))

$(target): $(cobj)
	echo [linking] $(target)
	$(CC) $(lflags) $^ -o $@


$(csrc:.c=.d):%.d:%.c
	$(CC) $(incl) -MM $^ >$@

ifneq "$(strip $(cdep))" ""
-include $(cdep)
endif

$(csrc:.c=.o):%.o:%.c
	@echo [$(CC)] $< '-->' $@; 
	$(CC) $(cflags) $< -o $@;

clean:
	rm -f $(target)
	rm -f $(cobj)
	rm -f $(cdep)


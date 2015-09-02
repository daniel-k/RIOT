APPLICATION = driver_at30tse75x
include ../Makefile.tests_common

BOARD = samr21-xpro

FEATURES_REQUIRED = periph_i2c

USEMODULE += at30tse75x
USEMODULE += uart0
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps

CFLAGS += -DDEVELHELP

include $(RIOTBASE)/Makefile.include

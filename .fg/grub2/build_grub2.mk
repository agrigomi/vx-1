include .fg/$(PROJECT)/_defaults_


.PHONY: all

all: $(DIR)/$(MAKEFILE)
	cd $(DIR) && $(MAKE) -s


$(DIR)/$(MAKEFILE): $(DIR)/$(CONFIGURE)
	cd $(DIR) && ./$(CONFIGURE)

$(DIR)/$(CONFIGURE): $(DIR)/$(AUTOGEN)
	cd $(DIR) && ./$(AUTOGEN)

$(DIR)/$(AUTOGEN):
	git clone $(URL)
	cd $(DIR) && git checkout origin/$(BRANCH) -b $(BRANCH)
clean:
	cd $(DIR) && $(MAKE) -s $@

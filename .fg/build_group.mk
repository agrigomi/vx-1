include .fg/_defaults_
include .fg/$(PROJECT)/_defaults_

project_config=.fg/$(PROJECT)/$(CONFIG)/_config_
-include $(project_config)

target_config=.fg/$(PROJECT)/$(CONFIG)/$(basename $(TARGET))/_config_
-include $(target_config)

#supress target PRE/POST build steps before include group config
PREBUILD=
POSTBUILD=
#set default linker flag for link relocatable
LINKER=ld
LINKER_FLAGS=-r
group_config=.fg/$(PROJECT)/$(CONFIG)/$(basename $(TARGET))/$(GROUP)._config_
-include $(group_config)

#read source files from group file
group_file=.fg/$(PROJECT)/$(CONFIG)/$(basename $(TARGET))/$(GROUP)

target_name=$(basename $(TARGET))
target_dir=$(OUTDIR)/$(PROJECT)/$(CONFIG)/$(target_name)
group_target=$(target_dir)/$(GROUP)$(OUTSUFFIX)
#read group sources
group_src=$(shell cat $(group_file))
group_out=$(addprefix $(target_dir)/$(GROUP)-, $(notdir $(group_src:$(SRCSUFFIX)=$(OUTSUFFIX))))
group_dep=$(addprefix $(target_dir)/$(GROUP)-, $(notdir $(group_src:$(SRCSUFFIX)=$(DEPSUFFIX))))

.PHONY: clean $(group_dep)

$(group_target): $(group_out) 
	@echo '    ' [$(LINKER)] $@
	$(LINKER) $(LINKER_FLAGS) $^ $(LINKER_OUTFLAG) $@
	$(POSTBUILD) PROJECT=$(PROJECT) TARGET=$(TARGET) CONFIG=$(CONFIG) GROUP=$(GROUP)

$(group_out): $(group_dep)
	for i in $(group_src); do \
		make $(MAKE_FLAGS) -f $(BUILD_FILE) PROJECT=$(PROJECT) TARGET=$(TARGET) CONFIG=$(CONFIG) GROUP=$(GROUP) SRC=$$i || exit; \
	done

$(group_dep): $(group_src) 
	for i in $(group_src); do \
		make $(MAKE_FLAGS) -f $(BUILD_DEPENDENCY) PROJECT=$(PROJECT) TARGET=$(TARGET) CONFIG=$(CONFIG) GROUP=$(GROUP) SRC=$$i || exit; \
	done

clean:
	rm -f $(group_out) $(group_target)


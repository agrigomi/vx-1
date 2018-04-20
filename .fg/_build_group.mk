include .fg/$(PROJECT)/_defaults_

project_config=.fg/$(PROJECT)/$(CONFIG)/_config_
-include $(project_config)

target_config=.fg/$(PROJECT)/$(CONFIG)/$(basename $(TARGET))/_config_
-include $(target_config)

#supress target PRE/POST build steps before include group config
PREBUILD=
POSTBUILD=
LINKER=ld
LINKER_FLAGS=-r
group_config=.fg/$(PROJECT)/$(CONFIG)/$(basename $(TARGET))/$(GROUP)._config_
-include $(group_config)

#read source files from group file
group_file=.fg/$(PROJECT)/$(CONFIG)/$(basename $(TARGET))/$(GROUP)

target_name=$(basename $(TARGET))
target_dir=$(OUTDIR)/$(target_name)/$(CONFIG)
target=$(target_dir)/$(GROUP)$(OUTSUFFIX)

src=$(shell cat $(group_file))
dep=$(src:$(SRCSUFFIX)=$(DEPSUFFIX))
obj=$(src:$(SRCSUFFIX)=$(OUTSUFFIX))

.PHONY: clean

dep_pcfg=
ifneq ("$(wildcard $(project_config))","")
	dep_pcfg=$(project_config)
endif
dep_tcfg=
ifneq ("$(wildcard $(target_config))","")
	dep_tcfg=$(target_config)
endif
dep_gcfg=
ifneq ("$(wildcard $(group_config))","")
	dep_gcfg=$(group_config)
endif


$(target): $(obj) $(dep)
	@echo '    ' [$(LINKER)] $@
	$(LINKER) $(LINKER_FLAGS) $(obj) $(LINKER_OUTFLAG) $@
	$(POSTBUILD) PROJECT=$(PROJECT) TARGET=$(TARGET) CONFIG=$(CONFIG) GROUP=$(GROUP)

$(dep): %$(DEPSUFFIX): %$(SRCSUFFIX)
	$(PREPROCESSOR) $(PREPROCESSOR_FLAGS) $< $(PREPROCESSOR_OUTFLAG) $(@:$(DEPSUFFIX)=$(OUTSUFFIX)) > $@
	
	
ifneq ("$(strip $(dep))", "")
-include $(dep)
endif


$(obj): %$(OUTSUFFIX): %$(SRCSUFFIX) $(group_file) $(dep_gcfg) $(dep_tcfg) $(dep_pcfg)
	@echo '    ' [$(COMPILER)] $@
	$(COMPILER) $(COMPILER_FLAGS) $< $(COMPILER_OUTFLAG) $@


clean:
	rm -f $(dep) $(obj) $(target)



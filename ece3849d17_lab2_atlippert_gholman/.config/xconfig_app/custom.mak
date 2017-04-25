## THIS IS A GENERATED FILE -- DO NOT EDIT
.configuro: .libraries,em3 linker.cmd package/cfg/app_pem3.oem3

linker.cmd: package/cfg/app_pem3.xdl
	$(SED) 's"^\"\(package/cfg/app_pem3cfg.cmd\)\"$""\"R:/Newfolder/ece3849d17_lab2_atlippert_gholman/.config/xconfig_app/\1\""' package/cfg/app_pem3.xdl > $@

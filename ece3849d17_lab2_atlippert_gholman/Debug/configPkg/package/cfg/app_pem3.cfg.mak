# invoke SourceDir generated makefile for app.pem3
app.pem3: .libraries,app.pem3
.libraries,app.pem3: package/cfg/app_pem3.xdl
	$(MAKE) -f R:\Newfolder\ece3849d17_lab2_atlippert_gholman/src/makefile.libs

clean::
	$(MAKE) -f R:\Newfolder\ece3849d17_lab2_atlippert_gholman/src/makefile.libs clean


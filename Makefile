IDIR = include
ODIR = build
CDIR = src
BDIR = bin
CC = g++
#CFLAGS = -I$(IDIR)
CFLAGS = $(patsubst %,-I%,$(CDIRS)) -I/Users/snotdaq/penn_daq2/libevent/include
LIBS = -L/Users/snotdaq/penn_daq2/libevent/lib -levent -lcurl -levent_pthreads -lpthread

_CDIRS = db core cont net xl3 mtc tests
CDIRS = $(patsubst %,$(CDIR)/%,$(_CDIRS))

vpath %.h $(CDIRS)
vpath %.cpp $(CDIRS)

_OBJ = Main.o NetUtils.o XL3Link.o GenericLink.o ControllerLink.o XL3Cmds.o Globals.o XL3Model.o DB.o Json.o Pouch.o MTCLink.o MTCCmds.o MTCModel.o FECTest.o MemTest.o BoardID.o CaldTest.o CGTTest.o ChinjScan.o CrateCBal.o DiscCheck.o FifoTest.o GTValidTest.o MbStabilityTest.o PedRun.o SeeReflection.o TriggerScan.o TTot.o VMon.o ZDisc.o RunPedestals.o FinalTest.o ECAL.o FindNoise.o DACSweep.o LocalVMon.o CreateFECDocs.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

_DEPS = $(_OBJ:.o=.h) DBTypes.h XL3PacketTypes.h MTCPacketTypes.h  XL3Registers.h UnpackBundles.h DacNumber.h MTCRegisters.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

#$(ODIR)/%.o: %.c $(DEPS)
$(ODIR)/%.o: %.cpp $(_DEPS)
	$(CC) -g -c -o $@ $< $(CFLAGS)

#$(IDIR)/%: %
#	cp $^ $(IDIR)/.

#all: penn_daq tut $(DEPS)
all: directories penn_daq tut

directories:
	test -d $(ODIR) || mkdir $(ODIR) 
	test -d $(BDIR) || mkdir $(BDIR) 
	test -d logs || mkdir logs 
	test -d macro || mkdir macro 

penn_daq: $(OBJ)
	$(CC) -g -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS) 

tut:
	python $(CDIR)/tut/tut_gen.py
	gcc -o $(BDIR)/tut $(CDIR)/tut/tut.c $(TUT_LIBRARY_PATH) $(CFLAGS) -L/usr/local/lib -lreadline -lncurses 
    
clean: 
	rm -f $(ODIR)/* $(BDIR)/*
#rm -f $(ODIR)/*.o core $(IDIR)/* $(BDIR)/*

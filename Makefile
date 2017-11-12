OUT = paxos
TEST = gendataset
CC = g++
OBJDIR = obj
SRCDIR = src
LOGDIR = log
INDIR = input
OUTDIR = output
INC = inc
CFLAGS = -I./$(INC) -g -D__USE_XOPEN2K8 -std=c++1z -O0 -D_LOGPAXOS -D_LOGMSG -D_NOBACKOFF
# By default looging is enabled and there is no back-off in the event of too many rejection.
# Logging and back-off is controlled by _LOGMSG, _LOGPAXOS and _NOBACKOFF macros.
LFLAGS = -pthread

_OBJS0 = main.o debug.o connection.o paxos.o conn_accept.o
_OBJS1 = client.o proposer.o acceptor.o learner.o receiver.o
_OBJ2 = proposer_action.o acceptor_action.o receiver_action.o
_OBJ3 = learner_action.o client_action.o
_OBJS = $(_OBJS0) $(_OBJS1) $(_OBJ2) $(_OBJ3)
OBJS = $(patsubst %,$(OBJDIR)/%,$(_OBJS))

_TOBJS = gendataset.o
TOBJS = $(patsubst %,$(OBJDIR)/%,$(_TOBJS))

programs : paxos
all : directories programs
default : paxos
directories : $(OBJDIR) $(LOGDIR) $(INDIR) $(OUTDIR)
test : gendataset

$(OBJDIR) :
	mkdir -p $(OBJDIR)

$(LOGDIR) :
	mkdir -p $(LOGDIR)

$(INDIR) :
	mkdir -p $(INDIR)
$(OUTDIR) :
	mkdir -p $(OUTDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

$(OUT): $(OBJS)
	$(CC) -o $(OUT) $^ $(LFLAGS)

$(TEST): $(TOBJS) 
	$(CC) -o $@ $^

.PHONY: clean

clean:
	rm -rf $(OBJDIR) $(LOGDIR) $(OUT) $(TEST) $(INDIR) $(OUTDIR)

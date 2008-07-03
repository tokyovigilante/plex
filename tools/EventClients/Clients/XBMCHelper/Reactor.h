#include <sys/select.h>
#include <pthread.h>

class Reactor
{
 public:
 
  enum Command
  {
    CmdExit,
    CmdData,
    CmdTimer,
    CmdTimerReset,
  };
 
  enum EventMask
  {
    NoEvent         = 0x00,  // Used internally.
    ReadMask        = 0x01,  // Socket has data that can be read.
    WriteMask       = 0x02,  // Socket is ready to be written to.
    ExceptMask      = 0x04,  // Socket has exception or OOB data.
    TickleMask      = 0x08,  // Reactor was tickled out of its loop.
  };

  //
  // Constructor.
  //
  Reactor()
    : _itsThread(0)
    , _isRunning(false)
    , _itsTimeout(0)
  {
    FD_ZERO(&_itsReadSet);
    FD_ZERO(&_itsWriteSet);
    FD_ZERO(&_itsExceptSet);

    // Tickle pipe.
    _itsTicklePipe[0] = -1;
    _itsTicklePipe[1] = -1;
  }

  //
  // Destructor.
  //
  virtual ~Reactor()
  {
    ::shutdown(_itsTicklePipe[0], 2);
    ::close(_itsTicklePipe[0]);
    ::shutdown(_itsTicklePipe[1], 2);
    ::close(_itsTicklePipe[1]);
  }

  void start()
  {
    if (::pipe(_itsTicklePipe) == -1)
      printf("Error creating tickle pipe = %d\n", errno);
    else
      addNotification(_itsTicklePipe[0], ReadMask);
    
    if (_itsThread == 0)
    {
      pthread_create(&_itsThread, NULL, Run, (void *)this);
      _isRunning = true;
    }
  }

  void stop()
  {
    _isRunning = false;
    sendMessage(CmdExit);
  }
  
  void sendMessage(Command cmd, int data=0)
  {
    // Tickle our tickle-port.
    unsigned char theCmd = cmd;
    if (::write(_itsTicklePipe[1], &theCmd, 1) == -1 ||
        ::write(_itsTicklePipe[1], &data,   4) == -1)
    {
      printf("Error tickling: %d.\n");
    }
  }

 protected:
 
   void addNotification(int theSocket, EventMask theMask)
   {
     //
     // Note that the mask coming in is overridding whatever mask was set
     // for the socket.
     //

   #define SET_EVENT_MASK(mask, set) \
     if (theMask & mask)             \
       FD_SET(theSocket, &set);      \
     else                            \
       FD_CLR(theSocket, &set);    

     SET_EVENT_MASK(ReadMask,   _itsReadSet);
     SET_EVENT_MASK(WriteMask,  _itsWriteSet);
     SET_EVENT_MASK(ExceptMask, _itsExceptSet);

     // Keep track of the maximum.
     if (theSocket > _itsMaxFD)
       _itsMaxFD = theSocket;
   }

  // Sets a timer.
  void setTimer(int ms)
  {
    sendMessage(CmdTimer, ms);
  }
  
  void resetTimer()
  {
    sendMessage(CmdTimerReset, 0);
  }

  // Runs in its own thread.
  void run()
  {
    while (_isRunning)
    {
      // Copy the fd_sets.
      fd_set readSet   = _itsReadSet;
      fd_set writeSet  = _itsWriteSet;
      fd_set exceptSet = _itsExceptSet;

      // Wait if we have a timeout.
      struct timeval timeOut;
      timeOut.tv_sec  = (_itsTimeout / 1000);
      timeOut.tv_usec = (_itsTimeout % 1000) * 1000;

      // Go see what's live.
      int n = ::select(_itsMaxFD + 1, &readSet, &writeSet, &exceptSet, (_itsTimeout != 0) ? &timeOut : 0);
  
      if (n == 0)
      {
        // The timeout occured, reset and call.
        _itsTimeout = 0;
        onTimeout();
      }
      else if (n > 0 && FD_ISSET(_itsTicklePipe[0], &readSet))
      {
        // Read the command byte and the parameter.
        unsigned char cmd = CmdExit;
        ::read(_itsTicklePipe[0], &cmd, 1);
        
        int data = 0;
        ::read(_itsTicklePipe[0], &data, 4);
        
        switch (cmd)
        {
          case CmdTimer:
            _itsTimeout = data;
            break;
          case CmdExit:
            _isRunning = false;
            break;
          case CmdTimerReset:
            _itsTimeout = 0;
            break;
          case CmdData:
            onCmd(data);
            break;
        }
      }
      else
      {
        printf("Reactor: error in select() - %d.\n", errno);
        sleep(1);
      }
    }
  }
  
 protected:
 
   virtual void onTimeout() = 0;
   virtual void onCmd(int data) = 0;
  
 private:
 
  static void* Run(void* param)
  {
    ((Reactor* )param)->run();
  }

  fd_set    _itsReadSet;
  fd_set    _itsWriteSet;
  fd_set    _itsExceptSet;
  bool      _isRunning;
  pthread_t _itsThread;
  int       _itsMaxFD;
  int       _itsTimeout;
  int       _itsTicklePipe[2];
};
// Dummy ATEM switcher class for debugging.
class ATEMext
{
public:
  ATEMext()
  : m_lastmillis(millis()),
    m_previewsource(1),
    m_keyerenabled(false),
    m_programsource(5),
    m_keyonair(false),
    m_auxsource(1),
    m_transitionposition(0),
    m_ftbpos(0),
    m_auto(false)
  {}
  ~ATEMext() {}

  void begin(IPAddress) {}
  void serialOutput(int) {}
  void connect() {}

  void runLoop()
  {
    unsigned long cmillis = millis();
    if ( cmillis - m_lastmillis > sfadespeed )
    {
      m_lastmillis = cmillis;
      if ( m_auto )
      {
        m_transitionposition += sfadestep;
        if ( m_transitionposition > 10000 )
        {
          m_transitionposition = 0;
          m_auto = false;
          switchSources();
        }
      }
      if ( m_ftbdirection != 0)
      {
        m_ftbpos += m_ftbdirection;
        if ( m_ftbpos > 10000 )
        {
          m_ftbpos = 10000;
          m_ftbdirection = 0;
        }
        else if ( m_ftbpos < 0 )
        {
          m_ftbpos = 0;
          m_ftbdirection = 0;
        }
        Serial.print("FTB ");
        Serial.println(m_ftbpos);
      }
    }
  }

  void switchSources()
  {
    auto temp = m_previewsource;
    m_previewsource = m_programsource;
    m_programsource = temp;
  }

  uint16_t getPreviewInputVideoSource(int)            { return m_previewsource; }
  void setPreviewInputVideoSource(int, int source)    { m_previewsource = source; }
  bool getKeyerFlyEnabled(int, int)                   { return m_keyerenabled; }
  void setKeyerFlyEnabled(int, int, bool enabled)     { m_keyerenabled = enabled; }
  uint16_t getProgramInputVideoSource(int)            { return m_programsource; }
  void setProgramInputVideoSource(int, int source)    { m_programsource = source; }
  bool getKeyerOnAirEnabled(int, int)                 { return m_keyonair; }
  void setKeyerOnAirEnabled(int, int, bool enabled)   { m_keyonair = enabled; }
  void setTransitionNextTransition(int, int value)    { m_transitionnexttransition = value; }
  uint16_t getTransitionNextTransition(int)           { return m_transitionnexttransition; }
  uint16_t getAuxSourceInput(int)                     { return m_auxsource; }
  void setAuxSourceInput(int, int source)             { m_auxsource = source; }
  uint16_t getTransitionPosition(int)                 { return m_transitionposition; }
  
  void setTransitionPosition(int, uint16_t value)     
  {
    if ( value >= 10000 )
    {
      switchSources();
      m_transitionposition = 0;
    }
    else
    {
      m_transitionposition = value;
    }
  }
  
  uint16_t getFadeToBlackStateInTransition(int)       { return m_ftbdirection != 0; }
  uint16_t getFadeToBlackStateFullyBlack(int)         { return m_ftbpos == 10000; }
  void performFadeToBlackME(int)                      { m_ftbdirection = (m_ftbpos == 0) ? sfadestep : -sfadestep; }
  void performAutoME(int)                             { m_auto = true; }
  
private:
  static const uint16_t       sfadestep  = 500;
  static const unsigned long  sfadespeed = 125;

  unsigned long m_lastmillis;
  uint16_t  m_previewsource;
  bool      m_keyerenabled;
  uint16_t  m_programsource;
  bool      m_keyonair;
  uint16_t  m_auxsource;
  int       m_transitionposition;
  int       m_ftbdirection;
  int       m_ftbpos;
  bool      m_auto;
  uint16_t  m_transitionnexttransition;
};

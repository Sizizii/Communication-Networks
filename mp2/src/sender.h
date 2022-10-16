class Sender;

class State{
public:
    /* init */
    State();
    State(Sender* sender);
    virtual ~State();

    /* operation */
    int state_type; // 0 for slow start, 1 for congestion avoid, 2 for fast recovery
    virtual void dupACK() = 0;
    virtual void newACK() = 0;
    virtual void timeOut() = 0;
    
protected:
    Sender *tcp_sender;
};

class FastRecovery: public State{
public:
    /* init */
    FastRecovery(Sender* sender);

    void dupACK();
    void newACK();
    void timeOut();
};

class CongestionAvoid: public State{
public:
    /* init */
    CongestionAvoid(Sender* sender);

    void dupACK();
    void newACK();
    void timeOut();
};

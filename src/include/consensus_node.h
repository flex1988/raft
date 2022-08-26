namespace raft
{

class ConsensusNode
{
    ConsensusNode() = default;

    ConsensusNode& operator=(const ConsensusNode&) = delete;

    virtual ~ConsensusNode();

    virtual void LeaderSendHeartBeats() = 0;

    virtual void AppendEntries() = 0;

};

}
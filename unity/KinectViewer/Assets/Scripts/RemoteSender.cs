using System.Net;

public class RemoteSender
{
    public IPEndPoint SenderEndPoint { get; private set; }
    public int SenderSessionId { get; private set; }
    public int ReceiverSessionId { get; private set; }

    public RemoteSender(IPEndPoint senderEndPoint, int senderSessionId, int receiverSessionId)
    {
        SenderEndPoint = senderEndPoint;
        SenderSessionId = senderSessionId;
        ReceiverSessionId = receiverSessionId;
    }
}
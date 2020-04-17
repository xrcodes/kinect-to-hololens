using System.Collections.Generic;

public class ControllerMessages
{
    public const int PORT = 3798;
}

public class ReceiverState
{
    public int sessionId;
    public string endPoint;

    public ReceiverState(int sessionId, string endPoint)
    {
        this.sessionId = sessionId;
        this.endPoint = endPoint;
    }
}

public class ViewerState
{
    public List<ReceiverState> receiverStates;

    public ViewerState(List<ReceiverState> receiverStates)
    {
        this.receiverStates = receiverStates;
    }
}
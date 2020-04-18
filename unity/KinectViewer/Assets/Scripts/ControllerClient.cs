using System.Collections;
using System.Collections.Generic;

public class ControllerClient
{
    public readonly int UserId;
    private TcpSocket tcpSocket;

    public ControllerClient(int userId, TcpSocket tcpSocket)
    {
        UserId = userId;
        this.tcpSocket = tcpSocket;
    }
}

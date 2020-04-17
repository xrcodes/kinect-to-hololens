using System.Collections;
using System.Collections.Generic;

public class ControllerClient
{
    private TcpSocket tcpSocket;

    public ControllerClient(TcpSocket tcpSocket)
    {
        this.tcpSocket = tcpSocket;
    }
}

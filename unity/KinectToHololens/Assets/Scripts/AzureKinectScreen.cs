using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class AzureKinectScreen : MonoBehaviour
{
    public AzureKinectCalibration Calibration { get; private set; }
    public AzureKinectScreen(AzureKinectCalibration calibration)
    {
        Calibration = calibration;
    }
}

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 Module:       FGLGear.cpp
 Author:       Jon S. Berndt
               Norman H. Princen
 Date started: 11/18/99
 Purpose:      Encapsulates the landing gear elements
 Called by:    FGAircraft

 ------------- Copyright (C) 1999  Jon S. Berndt (jsb@hal-pc.org) -------------

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 Place - Suite 330, Boston, MA  02111-1307, USA.

 Further information about the GNU General Public License can also be found on
 the world wide web at http://www.gnu.org.

FUNCTIONAL DESCRIPTION
--------------------------------------------------------------------------------

HISTORY
--------------------------------------------------------------------------------
11/18/99   JSB   Created
01/30/01   NHP   Extended gear model to properly simulate steering and braking

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include "FGLGear.h"

namespace JSBSim {

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
DEFINITIONS
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
GLOBAL DATA
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

static const char *IdSrc = "$Id: FGLGear.cpp,v 1.12 2005/12/17 22:06:57 jberndt Exp $";
static const char *IdHdr = ID_LGEAR;

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS IMPLEMENTATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

FGLGear::FGLGear(Element* el, FGFDMExec* fdmex, int number) : Exec(fdmex),
                 GearNumber(number)
{
  Element *force_table=0;
  string force_type="";

  kSpring = bDamp = bDampRebound = dynamicFCoeff = staticFCoeff = rollingFCoeff = maxSteerAngle = 0;
  sSteerType = sBrakeGroup = sSteerType = "";
  isRetractable = 0;

  name = el->GetAttributeValue("name");
  sContactType = el->GetAttributeValue("type");
  if (el->FindElement("spring_coeff"))
    kSpring = el->FindElementValueAsNumberConvertTo("spring_coeff", "LBS/FT");
  if (el->FindElement("damping_coeff"))
    bDamp   = el->FindElementValueAsNumberConvertTo("damping_coeff", "LBS/FT/SEC");

  if (el->FindElement("damping_coeff_rebound"))
    bDampRebound   = el->FindElementValueAsNumberConvertTo("damping_coeff_rebound", "LBS/FT/SEC");
  else
    bDampRebound   = bDamp;

  if (el->FindElement("dynamic_friction"))
    dynamicFCoeff = el->FindElementValueAsNumber("dynamic_friction");
  if (el->FindElement("static_friction"))
    staticFCoeff = el->FindElementValueAsNumber("static_friction");
  if (el->FindElement("rolling_friction"))
    rollingFCoeff = el->FindElementValueAsNumber("rolling_friction");
  if (el->FindElement("max_steer"))
    maxSteerAngle = el->FindElementValueAsNumberConvertTo("max_steer", "DEG");
  if (el->FindElement("retractable"))
    isRetractable = (int)el->FindElementValueAsNumber("retractable");

  ForceY_Table = 0;
  force_table = el->FindElement("table");
  while (force_table) {
    force_type = force_table->GetAttributeValue("type");
    if (force_type == "CORNERING_COEFF") {
      ForceY_Table = new FGTable(Exec->GetPropertyManager(), force_table);
    } else {
      cerr << "Undefined force table for " << name << " contact point" << endl;
    }
    force_table = el->FindNextElement("table");
  }

  sSteerType = el->FindElementValue("steer_type");
  sBrakeGroup = el->FindElementValue("brake_group");

  if (maxSteerAngle == 360) sSteerType = "CASTERED";
  else if (maxSteerAngle == 0.0) sSteerType = "FIXED";
  else sSteerType = "STEERABLE";

  Element* element = el->FindElement("location");
  if (element) vXYZ = element->FindElementTripletConvertTo("IN");
  else {cerr << "No location given for contact " << name << endl; exit(-1);}

  if      (sBrakeGroup == "LEFT"  ) eBrakeGrp = bgLeft;
  else if (sBrakeGroup == "RIGHT" ) eBrakeGrp = bgRight;
  else if (sBrakeGroup == "CENTER") eBrakeGrp = bgCenter;
  else if (sBrakeGroup == "NOSE"  ) eBrakeGrp = bgNose;
  else if (sBrakeGroup == "TAIL"  ) eBrakeGrp = bgTail;
  else if (sBrakeGroup == "NONE"  ) eBrakeGrp = bgNone;
  else if (sBrakeGroup.empty()    ) {eBrakeGrp = bgNone;
                                     sBrakeGroup = "NONE (defaulted)";}
  else {
    cerr << "Improper braking group specification in config file: "
         << sBrakeGroup << " is undefined." << endl;
  }

  if      (sSteerType == "STEERABLE") eSteerType = stSteer;
  else if (sSteerType == "FIXED"    ) eSteerType = stFixed;
  else if (sSteerType == "CASTERED" ) eSteerType = stCaster;
  else if (sSteerType.empty()       ) {eSteerType = stFixed;
                                       sSteerType = "FIXED (defaulted)";}
  else {
    cerr << "Improper steering type specification in config file: "
         << sSteerType << " is undefined." << endl;
  }

  GearUp = false;
  GearDown = true;
  Servicable = true;

// Add some AI here to determine if gear is located properly according to its
// brake group type ??

  State       = Exec->GetState();
  Aircraft    = Exec->GetAircraft();
  Propagate   = Exec->GetPropagate();
  Auxiliary   = Exec->GetAuxiliary();
  FCS         = Exec->GetFCS();
  MassBalance = Exec->GetMassBalance();

  WOW = lastWOW = true; // should the value be initialized to true?
  ReportEnable = true;
  FirstContact = false;
  StartedGroundRun = false;
  TakeoffReported = LandingReported = false;
  LandingDistanceTraveled = TakeoffDistanceTraveled = TakeoffDistanceTraveled50ft = 0.0;
  MaximumStrutForce = MaximumStrutTravel = 0.0;
  SideForce = RollingForce = 0.0;
  SinkRate = GroundSpeed = 0.0;

  vWhlBodyVec = MassBalance->StructuralToBody(vXYZ);

  vLocalGear = Propagate->GetTb2l() * vWhlBodyVec;

  compressLength  = 0.0;
  compressSpeed   = 0.0;
  brakePct        = 0.0;
  maxCompLen      = 0.0;

  WheelSlip = lastWheelSlip = 0.0;
  TirePressureNorm = 1.0;

  Debug(0);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGLGear::FGLGear(const FGLGear& lgear)
{
  GearNumber = lgear.GearNumber;
  State    = lgear.State;
  Aircraft = lgear.Aircraft;
  Propagate = lgear.Propagate;
  Auxiliary = lgear.Auxiliary;
  Exec     = lgear.Exec;
  FCS      = lgear.FCS;
  MassBalance = lgear.MassBalance;

  vXYZ = lgear.vXYZ;
  vMoment = lgear.vMoment;
  vWhlBodyVec = lgear.vWhlBodyVec;
  vLocalGear = lgear.vLocalGear;

  WOW                = lgear.WOW;
  lastWOW            = lgear.lastWOW;
  ReportEnable       = lgear.ReportEnable;
  FirstContact       = lgear.FirstContact;
  StartedGroundRun   = lgear.StartedGroundRun;
  LandingDistanceTraveled   = lgear.LandingDistanceTraveled;
  TakeoffDistanceTraveled   = lgear.TakeoffDistanceTraveled;
  TakeoffDistanceTraveled50ft   = lgear.TakeoffDistanceTraveled50ft;
  MaximumStrutForce  = lgear.MaximumStrutForce;
  MaximumStrutTravel = lgear.MaximumStrutTravel;
  SideForce          = lgear.SideForce;
  RollingForce       = lgear.RollingForce;

  kSpring         = lgear.kSpring;
  bDamp           = lgear.bDamp;
  bDampRebound    = lgear.bDampRebound;
  compressLength  = lgear.compressLength;
  compressSpeed   = lgear.compressSpeed;
  staticFCoeff    = lgear.staticFCoeff;
  dynamicFCoeff   = lgear.dynamicFCoeff;
  rollingFCoeff   = lgear.rollingFCoeff;
  brakePct        = lgear.brakePct;
  maxCompLen      = lgear.maxCompLen;
  SinkRate        = lgear.SinkRate;
  GroundSpeed     = lgear.GroundSpeed;
  LandingReported = lgear.LandingReported;
  TakeoffReported = lgear.TakeoffReported;
  name            = lgear.name;
  sSteerType      = lgear.sSteerType;
  sRetractable    = lgear.sRetractable;
  sContactType    = lgear.sContactType;
  sBrakeGroup     = lgear.sBrakeGroup;
  eSteerType      = lgear.eSteerType;
  eBrakeGrp       = lgear.eBrakeGrp;
  maxSteerAngle   = lgear.maxSteerAngle;
  isRetractable   = lgear.isRetractable;
  GearUp          = lgear.GearUp;
  GearDown        = lgear.GearDown;
  WheelSlip       = lgear.WheelSlip;
  lastWheelSlip   = lgear.lastWheelSlip;
  TirePressureNorm = lgear.TirePressureNorm;
  Servicable      = lgear.Servicable;
  ForceY_Table    = lgear.ForceY_Table;
  CosWheel        = lgear.CosWheel;
  SinWheel        = lgear.SinWheel;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGLGear::~FGLGear()
{
  Debug(1);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGColumnVector3& FGLGear::Force(void)
{
  FGColumnVector3 normal, cvel;
  FGLocation contact, gearLoc;
  double t = Exec->GetState()->Getsim_time();

  vForce.InitMatrix();
  vMoment.InitMatrix();

  if (isRetractable) ComputeRetractionState();

  if (GearUp) return vForce;

  vWhlBodyVec = MassBalance->StructuralToBody(vXYZ); // Get wheel in body frame
  vLocalGear = Propagate->GetTb2l() * vWhlBodyVec; // Get local frame wheel location

  gearLoc = Propagate->GetLocation().LocalToLocation(vLocalGear);
  compressLength = -Exec->GetGroundCallback()->GetAGLevel(t, gearLoc, contact, normal, cvel);

  // The compression length is measured in the Z-axis, only, at this time.

  if (compressLength > 0.00) {

    WOW = true;

    // [The next equation should really use the vector to the contact patch of
    // the tire including the strut compression and not the original vWhlBodyVec.]

    vWhlVelVec      =  Propagate->GetTb2l() * (Propagate->GetPQR() * vWhlBodyVec);
    vWhlVelVec     +=  Propagate->GetVel() - cvel;
    compressSpeed   =  vWhlVelVec(eZ);

    InitializeReporting();
    ComputeBrakeForceCoefficient();
    ComputeSteeringAngle();
    ComputeSlipAngle();
    ComputeSideForceCoefficient();
    ComputeVerticalStrutForce();

    // Compute the forces in the wheel ground plane.

    RollingForce = (1.0 - TirePressureNorm) * 30
                   + vLocalForce(eZ) * BrakeFCoeff * (RollingWhlVel>=0?1.0:-1.0);
    SideForce    = vLocalForce(eZ) * FCoeff;

    // Transform these forces back to the local reference frame.

    vLocalForce(eX) = RollingForce*CosWheel - SideForce*SinWheel;
    vLocalForce(eY) = SideForce*CosWheel    + RollingForce*SinWheel;

    // Transform the forces back to the body frame and compute the moment.

    vForce  = Propagate->GetTl2b() * vLocalForce;
    vMoment = vWhlBodyVec * vForce;

  } else { // Gear is NOT compressed

    WOW = false;
    compressLength = 0.0;

    // Return to neutral position between 1.0 and 0.8 gear pos.
    SteerAngle *= max(FCS->GetGearPos()-0.8, 0.0)/0.2;

    ResetReporting();
  }

  ReportTakeoffOrLanding();
  CrashDetect();

  return vForce;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGLGear::ComputeRetractionState(void)
{
  if (FCS->GetGearPos() < 0.01) {
    GearUp   = true;
    GearDown = false;
  } else if (FCS->GetGearPos() > 0.99) {
    GearDown = true;
    GearUp   = false;
  } else {
    GearUp   = false;
    GearDown = false;
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGLGear::ComputeSlipAngle(void)
{
  // Transform the wheel velocities from the local axis system to the wheel axis system.

  RollingWhlVel = vWhlVelVec(eX)*CosWheel + vWhlVelVec(eY)*SinWheel;
  SideWhlVel    = vWhlVelVec(eY)*CosWheel - vWhlVelVec(eX)*SinWheel;

  // Calculate tire slip angle.

  if (RollingWhlVel == 0.0 && SideWhlVel == 0.0) {
    WheelSlip = 0.0;
  } else if (fabs(RollingWhlVel) < 1.0) {
    WheelSlip = SteerAngle*radtodeg;
  } else {
    WheelSlip = radtodeg*atan2(SideWhlVel, fabs(RollingWhlVel));
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// Compute the steering angle in any case.
// This will also make sure that animations will look right.

void FGLGear::ComputeSteeringAngle(void)
{
  switch (eSteerType) {
  case stSteer:
    SteerAngle = degtorad * FCS->GetSteerPosDeg(GearNumber);
    break;
  case stFixed:
    SteerAngle = 0.0;
    break;
  case stCaster:
    // This is not correct for castering gear. Should make steer angle parallel
    // to the actual velocity vector of the wheel, given aircraft velocity vector
    // and omega.
    SteerAngle = 0.0;
    break;
  default:
    cerr << "Improper steering type membership detected for this gear." << endl;
    break;
  }

  SinWheel      = sin(Propagate->GetEuler(ePsi) + SteerAngle);
  CosWheel      = cos(Propagate->GetEuler(ePsi) + SteerAngle);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// Reset reporting functionality after takeoff

void FGLGear::ResetReporting(void)
{
  if (Propagate->GetDistanceAGL() > 200.0) {
    FirstContact = false;
    StartedGroundRun = false;
    LandingReported = false;
    LandingDistanceTraveled = 0.0;
    MaximumStrutForce = MaximumStrutTravel = 0.0;
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGLGear::InitializeReporting(void)
{
  // If this is the first time the wheel has made contact, remember some values
  // for later printout.

  if (!FirstContact) {
    FirstContact  = true;
    SinkRate      =  compressSpeed;
    GroundSpeed   =  Propagate->GetVel().Magnitude();
    TakeoffReported = false;
  }

  // If the takeoff run is starting, initialize.

  if ((Propagate->GetVel().Magnitude() > 0.1) &&
      (FCS->GetBrake(bgLeft) == 0) &&
      (FCS->GetBrake(bgRight) == 0) &&
      (FCS->GetThrottlePos(0) == 1) && !StartedGroundRun)
  {
    TakeoffDistanceTraveled = 0;
    TakeoffDistanceTraveled50ft = 0;
    StartedGroundRun = true;
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// Takeoff and landing reporting functionality

void FGLGear::ReportTakeoffOrLanding(void)
{
  double deltaT = State->Getdt()*Exec->GetGroundReactions()->GetRate();

  if (FirstContact) LandingDistanceTraveled += Auxiliary->GetVground()*deltaT;

  if (StartedGroundRun) {
     TakeoffDistanceTraveled50ft += Auxiliary->GetVground()*deltaT;
    if (WOW) TakeoffDistanceTraveled += Auxiliary->GetVground()*deltaT;
  }

  if (ReportEnable && Auxiliary->GetVground() <= 0.05 && !LandingReported) {
    if (debug_lvl > 0) Report(erLand);
  }

  if (ReportEnable && !TakeoffReported &&
     (vLocalGear(eZ) - Propagate->GetDistanceAGL()) < -50.0)
  {
    if (debug_lvl > 0) Report(erTakeoff);
  }

  if (lastWOW != WOW) PutMessage("GEAR_CONTACT: " + name, WOW);
  lastWOW = WOW;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// Crash detection logic (really out-of-bounds detection)

void FGLGear::CrashDetect(void)
{
  if (compressLength > 500.0 ||
      vForce.Magnitude() > 100000000.0 ||
      vMoment.Magnitude() > 5000000000.0 ||
      SinkRate > 1.4666*30)
  {
    PutMessage("Crash Detected: Simulation FREEZE.");
    State->SuspendIntegration();
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// The following needs work regarding friction coefficients and braking and
// steering The BrakeFCoeff formula assumes that an anti-skid system is used.
// It also assumes that we won't be turning and braking at the same time.
// Will fix this later.
// [JSB] The braking force coefficients include normal rolling coefficient +
// a percentage of the static friction coefficient based on braking applied.

void FGLGear::ComputeBrakeForceCoefficient(void)
{
  switch (eBrakeGrp) {
  case bgLeft:
    BrakeFCoeff =  ( rollingFCoeff*(1.0 - FCS->GetBrake(bgLeft)) +
                     staticFCoeff*FCS->GetBrake(bgLeft) );
    break;
  case bgRight:
    BrakeFCoeff =  ( rollingFCoeff*(1.0 - FCS->GetBrake(bgRight)) +
                     staticFCoeff*FCS->GetBrake(bgRight) );
    break;
  case bgCenter:
    BrakeFCoeff =  ( rollingFCoeff*(1.0 - FCS->GetBrake(bgCenter)) +
                     staticFCoeff*FCS->GetBrake(bgCenter) );
    break;
  case bgNose:
    BrakeFCoeff =  ( rollingFCoeff*(1.0 - FCS->GetBrake(bgCenter)) +
                     staticFCoeff*FCS->GetBrake(bgCenter) );
    break;
  case bgTail:
    BrakeFCoeff =  ( rollingFCoeff*(1.0 - FCS->GetBrake(bgCenter)) +
                     staticFCoeff*FCS->GetBrake(bgCenter) );
    break;
  case bgNone:
    BrakeFCoeff =  rollingFCoeff;
    break;
  default:
    cerr << "Improper brake group membership detected for this gear." << endl;
    break;
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// Compute the sideforce coefficients using similar assumptions to LaRCSim for now.
// Allow a maximum of 10 degrees tire slip angle before wheel slides.  At that point,
// transition from static to dynamic friction.  There are more complicated formulations
// of this that avoid the discrete jump (similar to Pacejka).  Will fix this later.

void FGLGear::ComputeSideForceCoefficient(void)
{
  if (ForceY_Table) {

    FCoeff = ForceY_Table->GetValue(WheelSlip);

  } else {

    if (fabs(WheelSlip) <= 10.0) {
      FCoeff = staticFCoeff*WheelSlip/10.0;
    } else if (fabs(WheelSlip) <= 40.0) {
      FCoeff = (dynamicFCoeff*(fabs(WheelSlip) - 10.0)/10.0
                + staticFCoeff*(40.0 - fabs(WheelSlip))/10.0)*(WheelSlip>=0?1.0:-1.0);
    } else {
      FCoeff = dynamicFCoeff*(WheelSlip>=0?1.0:-1.0);
    }
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// Compute the vertical force on the wheel using square-law damping (per comment
// in paper AIAA-2000-4303 - see header prologue comments). We might consider
// allowing for both square and linear damping force calculation. Also need to
// possibly give a "rebound damping factor" that differs from the compression
// case.

void FGLGear::ComputeVerticalStrutForce(void)
{
  double springForce = 0;
  double dampForce = 0;

  springForce = -compressLength * kSpring;

  if (compressSpeed >= 0.0) {
    dampForce   = -compressSpeed * bDamp;
  } else {
    dampForce   = -compressSpeed * bDampRebound;
  }
  vLocalForce(eZ) =  min(springForce + dampForce, (double)0.0);

  // Remember these values for reporting
  MaximumStrutForce = max(MaximumStrutForce, fabs(vLocalForce(eZ)));
  MaximumStrutTravel = max(MaximumStrutTravel, fabs(compressLength));
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGLGear::bind(void)
{
  char property_name[80];
  snprintf(property_name, 80, "gear/unit[%d]/slip-angle-deg", GearNumber);
  Exec->GetPropertyManager()->Tie( property_name, &WheelSlip );
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGLGear::unbind(void)
{
  char property_name[80];
  snprintf(property_name, 80, "gear/unit[%d]/slip-angle-deg", GearNumber);
  Exec->GetPropertyManager()->Untie( property_name );
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGLGear::Report(ReportType repType)
{
  switch(repType) {
  case erLand:
    cout << endl << "Touchdown report for " << name << endl;
    cout << "  Sink rate at contact:  " << SinkRate                << " fps,    "
                                << SinkRate*0.3048          << " mps"     << endl;
    cout << "  Contact ground speed:  " << GroundSpeed*.5925       << " knots,  "
                                << GroundSpeed*0.3048       << " mps"     << endl;
    cout << "  Maximum contact force: " << MaximumStrutForce       << " lbs,    "
                                << MaximumStrutForce*4.448  << " Newtons" << endl;
    cout << "  Maximum strut travel:  " << MaximumStrutTravel*12.0 << " inches, "
                                << MaximumStrutTravel*30.48 << " cm"      << endl;
    cout << "  Distance traveled:     " << LandingDistanceTraveled        << " ft,     "
                                << LandingDistanceTraveled*0.3048  << " meters"  << endl;
    LandingReported = true;
    break;
  case erTakeoff:
    cout << endl << "Takeoff report for " << name << endl;
    cout << "  Distance traveled:                " << TakeoffDistanceTraveled
         << " ft,     " << TakeoffDistanceTraveled*0.3048  << " meters"  << endl;
    cout << "  Distance traveled (over 50'):     " << TakeoffDistanceTraveled50ft
         << " ft,     " << TakeoffDistanceTraveled50ft*0.3048 << " meters" << endl;
    TakeoffReported = true;
    break;
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//    The bitmasked value choices are as follows:
//    unset: In this case (the default) JSBSim would only print
//       out the normally expected messages, essentially echoing
//       the config files as they are read. If the environment
//       variable is not set, debug_lvl is set to 1 internally
//    0: This requests JSBSim not to output any messages
//       whatsoever.
//    1: This value explicity requests the normal JSBSim
//       startup messages
//    2: This value asks for a message to be printed out when
//       a class is instantiated
//    4: When this value is set, a message is displayed when a
//       FGModel object executes its Run() method
//    8: When this value is set, various runtime state variables
//       are printed out periodically
//    16: When set various parameters are sanity checked and
//       a message is printed out when they go out of bounds

void FGLGear::Debug(int from)
{
  if (debug_lvl <= 0) return;

  if (debug_lvl & 1) { // Standard console startup message output
    if (from == 0) { // Constructor - loading and initialization
      cout << "    " << sContactType << " " << name          << endl;
      cout << "      Location: "         << vXYZ          << endl;
      cout << "      Spring Constant:  " << kSpring       << endl;
      cout << "      Damping Constant: " << bDamp         << endl;
      cout << "      Dynamic Friction: " << dynamicFCoeff << endl;
      cout << "      Static Friction:  " << staticFCoeff  << endl;
      if (sContactType == "BOGEY") {
        cout << "      Rolling Friction: " << rollingFCoeff << endl;
        cout << "      Steering Type:    " << sSteerType    << endl;
        cout << "      Grouping:         " << sBrakeGroup   << endl;
        cout << "      Max Steer Angle:  " << maxSteerAngle << endl;
        cout << "      Retractable:      " << isRetractable  << endl;
      }
    }
  }
  if (debug_lvl & 2 ) { // Instantiation/Destruction notification
    if (from == 0) cout << "Instantiated: FGLGear" << endl;
    if (from == 1) cout << "Destroyed:    FGLGear" << endl;
  }
  if (debug_lvl & 4 ) { // Run() method entry print for FGModel-derived objects
  }
  if (debug_lvl & 8 ) { // Runtime state variables
  }
  if (debug_lvl & 16) { // Sanity checking
  }
  if (debug_lvl & 64) {
    if (from == 0) { // Constructor
      cout << IdSrc << endl;
      cout << IdHdr << endl;
    }
  }
}

} // namespace JSBSim

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 Module:       FGFCSComponent.cpp
 Author:       Jon S. Berndt
 Date started: 11/1999

 ------------- Copyright (C) 2000 -------------

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

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
COMMENTS, REFERENCES,  and NOTES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include "FGFCSComponent.h"

namespace JSBSim {

static const char *IdSrc = "$Id: FGFCSComponent.cpp,v 1.6 2005/09/10 13:00:35 jberndt Exp $";
static const char *IdHdr = ID_FCSCOMPONENT;

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS IMPLEMENTATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

FGFCSComponent::FGFCSComponent(FGFCS* _fcs, Element* element) : fcs(_fcs)
{
  Element *input_element, *clip_el;
  Input = Output = clipmin = clipmax = 0.0;
  OutputNode = treenode = 0;
  ClipMinPropertyNode = ClipMaxPropertyNode = 0;
  IsOutput   = clip = false;
  string input, clip_string;

  PropertyManager = fcs->GetPropertyManager();
  Type = element->GetAttributeValue("type");
  Name = element->GetAttributeValue("name");

  input_element = element->FindElement("input");
  while (input_element) {
    input = input_element->GetDataLine();
    if (input[0] == '-') {
      InputSigns.push_back(-1.0);
      input.erase(0,1);
    } else {
      InputSigns.push_back( 1.0);
    }
    InputNodes.push_back( resolveSymbol(input) );
    input_element = element->FindNextElement("input");
  }

  if (element->FindElement("output")) {
    IsOutput = true;
    OutputNode = PropertyManager->GetNode( element->FindElementValue("output") );
    if (!OutputNode) {
      cerr << endl << "  Unable to process property: " << element->FindElementValue("output") << endl;
      throw(string("Invalid output property name in flight control definition"));
    }
  }

  clip_el = element->FindElement("clipto");
  if (clip_el) {
    clip_string = clip_el->FindElementValue("min");
    if (clip_string.find_first_not_of("+-.0123456789") != string::npos) { // it's a property
      ClipMinPropertyNode = PropertyManager->GetNode( clip_string );
    } else {
      clipmin = clip_el->FindElementValueAsNumber("min");
    }
    clip_string = clip_el->FindElementValue("max");
    if (clip_string.find_first_not_of("+-.0123456789") != string::npos) { // it's a property
      ClipMaxPropertyNode = PropertyManager->GetNode( clip_string );
    } else {
      clipmax = clip_el->FindElementValueAsNumber("max");
    }
    clip = true;
  }

  Debug(0);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGFCSComponent::~FGFCSComponent()
{
//  string tmp = "fcs/" + PropertyManager->mkPropertyName(Name, true);
//  PropertyManager->Untie( tmp);

  Debug(1);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFCSComponent::SetOutput(void)
{
  OutputNode->setDoubleValue(Output);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGFCSComponent::Run(void)
{
  return true;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFCSComponent::Clip(void)
{
  if (clip) {
    if (ClipMinPropertyNode != 0) clipmin = ClipMinPropertyNode->getDoubleValue();
    if (ClipMaxPropertyNode != 0) clipmax = ClipMaxPropertyNode->getDoubleValue();
    if (Output > clipmax)      Output = clipmax;
    else if (Output < clipmin) Output = clipmin;
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGPropertyManager* FGFCSComponent::resolveSymbol(string token)
{
  string prop;
  FGPropertyManager* tmp = PropertyManager->GetNode(token,false);
  if (!tmp) {
    if (token.find("/") == token.npos) prop = "model/" + token;
    cerr << "Creating new property " << prop << endl;
    tmp = PropertyManager->GetNode(token,true);
  }
  return tmp;
}


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGFCSComponent::bind(void)
{
  string tmp = "fcs/" + PropertyManager->mkPropertyName(Name, true);
  PropertyManager->Tie( tmp, this, &FGFCSComponent::GetOutput);
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

void FGFCSComponent::Debug(int from)
{
  if (debug_lvl <= 0) return;

  if (debug_lvl & 1) { // Standard console startup message output
    if (from == 0) {
      cout << endl << "    Loading Component \"" << Name
                   << "\" of type: " << Type << endl;
    }
  }
  if (debug_lvl & 2 ) { // Instantiation/Destruction notification
    if (from == 0) cout << "Instantiated: FGFCSComponent" << endl;
    if (from == 1) cout << "Destroyed:    FGFCSComponent" << endl;
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
}
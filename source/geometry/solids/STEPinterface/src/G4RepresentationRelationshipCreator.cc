#include "G4RepresentationRelationshipCreator.hh"
#include "G4PlacementVector.hh"
#include "STEPcomplex.h"

G4RepresentationRelationshipCreator G4RepresentationRelationshipCreator::csc;
G4int G4RepresentationRelationshipCreator::placeCount=0;
G4RepresentationRelationshipCreator::G4RepresentationRelationshipCreator(){G4GeometryTable::RegisterObject(this);}

G4RepresentationRelationshipCreator::~G4RepresentationRelationshipCreator(){}

void G4RepresentationRelationshipCreator::CreateG4Geometry(STEPentity& Ent)
{

  STEPcomplex* complexEnt = (STEPcomplex*)&Ent;
  void* tmp=0;
  STEPentity* subEnt=0;
  STEPentity* subEnt2=0;  
  G4String attrName;
  STEPattribute *Attr;

  G4PlacementVector*   itemDefPlaces;
  G4PlacementVector*   places;
  G4PlacedSolid*       sld;
  G4PlacedSolid*       tmpsld; 
  G4PlacedSolidVector* sldV = new G4PlacedSolidVector();
  G4PlacedSolidVector* tmpsldV;

  if(complexEnt->EntityExists("Representation_Relationship"))
  {
    subEnt = complexEnt->EntityPart("Representation_Relationship");

    // get subparts    
    attrName = "rep_1";
    Attr = GetNamedAttribute(attrName, *subEnt);
    subEnt2 = *Attr->ptr.c;
    places = (G4PlacementVector*)G4GeometryTable::CreateObject(*subEnt2);

    attrName = "rep_2";
    Attr = GetNamedAttribute(attrName, *subEnt);
    subEnt2 = *Attr->ptr.c;
    tmpsldV = (G4PlacedSolidVector*)G4GeometryTable::CreateObject(*subEnt2);
  }
  
  if( complexEnt->EntityExists
      ("Representation_Relationship_With_Transformation") )
  {
    subEnt = complexEnt->EntityPart
      ("Representation_Relationship_With_Transformation");
      
    // get subparts
    attrName = "transformation_operator";
    Attr = GetNamedAttribute(attrName, *subEnt);
    SdaiSelect* sel = Attr->ptr.sh;
    
    G4String underlyingTypeName(G4String(sel->UnderlyingTypeName()));
    G4String itemDefined("Item_Defined_Transformation");
    
    if(underlyingTypeName == itemDefined)
    {
      G4String koe;
      SdaiTransformation* transf = (SdaiTransformation*)sel;
      
      SdaiItem_defined_transformation* iTransf = 
	transf->operator SdaiItem_defined_transformationH();
	  
      SdaiRepresentation_itemH repItem1 = iTransf->Transform_item_1();
      SdaiRepresentation_itemH repItem2 = iTransf->Transform_item_2();
      
      subEnt = (STEPentity*)repItem1;
      itemDefPlaces = new G4PlacementVector();
      itemDefPlaces->append( ( (G4Axis2Placement3D*)G4GeometryTable::
			       CreateObject(*subEnt)                  ) );
      
      subEnt  = (STEPentity*)repItem2;
      itemDefPlaces->append( ( (G4Axis2Placement3D*)G4GeometryTable::
			       CreateObject(*subEnt)                  ) );
    }
    else
    {  
    }
      
    //      subEnt2 = *Attr->ptr.c;
    //      G4GeometryTable::CreateObject(*subEnt2);
  }
  
  if(complexEnt->EntityExists("Shape_Representation_Relationship"))
    subEnt = complexEnt->EntityPart("Shape_Representation_Relationship");


  G4Axis2Placement3D* place = (G4Axis2Placement3D*)places->at(placeCount);
  placeCount++;
  
  // the following line is probably wrong. The combination of
  // the two transformations should be taken (?)
  //G4PlacedSolid* ps=new G4PlacedSolid((G4BREPSolid*)sld->GetSolid(), place);
  
  // place the advanced BREP, so place each manifold solids
  G4int entr = tmpsldV->entries();
  for(G4int a = 0; a < entr; a++)
  {
    tmpsld = tmpsldV->at(a);
    sld = new G4PlacedSolid((G4BREPSolid*)tmpsld->GetSolid(), place);
    sldV->append(sld);
  }

  createdObject = sldV;
}

void G4RepresentationRelationshipCreator::CreateSTEPGeometry(void* G4obj)
{

}
/*=========================================================================

 Program: FEMuS
 Module: Mesh
 Authors: Eugenio Aulisa

 Copyright (c) FEMuS
 All rights reserved.

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef __femus_mesh_Mesh_hpp__
#define __femus_mesh_Mesh_hpp__

//----------------------------------------------------------------------------
// includes :
//----------------------------------------------------------------------------
#include "Elem.hpp"
#include "Solution.hpp"
#include "ElemType.hpp"
#include "ElemTypeEnum.hpp"
#include "ParallelObject.hpp"
#include <assert.h>

#include "vector"
#include "map"

namespace femus {



using std::vector;
class Solution;

/**
 * The mesh class
*/

class Mesh : public ParallelObject {

public:

    /** Constructor */
    explicit
    Mesh();

    /** destructor */
    ~Mesh();

    /** Print the mesh info for this level */
    void PrintInfo();

    /** Get the dof number for the element -type- */
    unsigned GetTotalNumberOfDofs(const unsigned &type) const {
      return _dofOffset[type][_nprocs];
    }

    /** Set the number of nodes */
    void SetNumberOfNodes(const unsigned &nnodes) {
      _nnodes = nnodes;
    };

    /** Get the number of nodes */
    unsigned GetNumberOfNodes() const {
      return _nnodes;
    }

    /** Set the number of element */
    void SetNumberOfElements(const unsigned &nelem) {
      _nelem = nelem;
    };

    /** Get the number of element */
    unsigned GetNumberOfElements() const {
      return _nelem;
    }

    /** Get if element is refined*/
    short unsigned GetRefinedElementIndex(const unsigned &iel) const;

    /** Get element group*/
    short unsigned GetElementGroup(const unsigned &iel) const;

    /** Get element material*/
    short unsigned GetElementMaterial(const unsigned &iel) const;

    /** Get element type*/
    short unsigned GetElementType(const unsigned &iel) const;

    /** Only for parallel */
    unsigned GetElementDofNumber(const unsigned &iel, const unsigned &type) const {
      return el->GetNVE(GetElementType(iel), type);
    }
    
    /** Only for parallel */
    const unsigned GetElementFaceType(const unsigned &kel, const unsigned &jface) const{
      unsigned kelt = GetElementType(kel);
      const unsigned FELT[6][2]= {{3,3},{4,4},{3,4},{5,5},{5,5},{6,6}};
      const unsigned felt = FELT[kelt][jface >= GetElementFaceNumber(kel,0)];
      return felt;
    }
    
    /** Only for parallel */
    unsigned GetLocalFaceVertexIndex(const unsigned &iel, const unsigned &iface, const unsigned &jnode) const {
      return el->GetIG(GetElementType(iel), iface, jnode);
    }
    
    
    /** Only for parallel */
    unsigned GetElementFaceDofNumber(const unsigned &iel, const unsigned jface, const unsigned &type) const {
      assert( type < 3 );
      return el->GetNFACENODES(GetElementType(iel), jface, type);
    }
    
    /** Only for parallel */
    unsigned GetElementFaceNumber(const unsigned &iel, const unsigned &type=1) const {
      return el->GetNFC(GetElementType(iel), type);
    }
    
    /** Set the grid number */
    void SetLevel(const unsigned &i) {
        _level=i;
    };

    /** Get the grid number */
    unsigned GetLevel() const {
      return _level;
    }

    /** Set the dimension of the problem (1D, 2D, 3D) */
    void SetDimension(const unsigned &dim) {
      Mesh::_dimension = dim;
      Mesh::_ref_index = pow(2,Mesh::_dimension);  // 8*DIM[2]+4*DIM[1]+2*DIM[0];
      Mesh::_face_index = pow(2,Mesh::_dimension-1u);
    }


    /** Get the dimension of the problem (1D, 2D, 3D) */
    const unsigned GetDimension() const {
      return Mesh::_dimension;
    }

    /** To be added*/
    const unsigned GetRefIndex() const {
      return Mesh::_ref_index;
    }

//     /** Get the metis dof from the gambit dof */
//     unsigned GetSolutionDof(const unsigned &inode, const short unsigned &solType) const {
//       return IS_Gmt2Mts_dof[solType][inode];
//     }

    unsigned GetSolutionDof(const unsigned &i, const unsigned &iel, const short unsigned &solType) const;

    /** Performs a bisection search to find the processor of the given dof */
    unsigned IsdomBisectionSearch(const unsigned &dof, const short unsigned &solType) const;

    /** To be added */
    const unsigned GetFaceIndex() const {
      return Mesh::_face_index;
    }

    /** Allocate memory for adding fluid or solid mark */
    void AllocateAndMarkStructureNode();


    /** To be Added */
    void SetFiniteElementPtr(const elem_type* otheFiniteElement[6][5]);

    /** Generate mesh functions */

    /** This function generates the coarse mesh level, $l_0$, from an input mesh file */
    void ReadCoarseMesh(const std::string& name, const double Lref, std::vector<bool> &_finiteElement_flag);

    /** This function generates a coarse box mesh */
    void GenerateCoarseBoxMesh(const unsigned int nx,
                               const unsigned int ny,
                               const unsigned int nz,
                               const double xmin, const double xmax,
                               const double ymin, const double ymax,
                               const double zmin, const double zmax,
                               const ElemType type, std::vector<bool> &type_elem_flag);


    /** To be added */
    void FillISvector(vector < int > &epart);

    /** To be added */
    void Buildkel();

    /** To be added */
    void BuildAdjVtx();


    // member data
    Solution* _topology;
    const elem_type *_finiteElement[6][5];

    vector < unsigned > _elementOffset;
    vector < unsigned > _ownSize[5];
    vector < unsigned > _dofOffset[5];
    vector< vector < int > > _ghostDofs[5];

    elem *el;  // topology object
    static bool (* _SetRefinementFlag)(const std::vector < double >& x,
                                       const int &ElemGroupNumber,const int &level);
    static bool _IsUserRefinementFunctionDefined;
    std::map<unsigned int, std::string> _boundaryinfo;

    /** Get the projection matrix between Lagrange FEM at the same level mesh*/
    SparseMatrix* GetQitoQjProjection(const unsigned& itype, const unsigned& jtype);

    /** Get the coarse to the fine projection matrix*/
    SparseMatrix* GetCoarseToFineProjection(const unsigned& solType);

    /** Set the coarser mesh from which this mesh is generated */
    void SetCoarseMesh( Mesh* otherCoarseMsh ){
      _coarseMsh = otherCoarseMsh;
    };



    const unsigned GetXIndex()          const { return _xIndex; };
    const unsigned GetYIndex()          const { return _yIndex; };
    const unsigned GetZIndex()          const { return _zIndex; };
    const unsigned GetAmrIndex()        const { return _amrIndex; };
    const unsigned GetMaterialIndex()   const { return _materialIndex; };
    const unsigned GetGroupIndex()      const { return _groupIndex; };
    const unsigned GetTypeIndex()       const { return _typeIndex; };

private:
    /** Coarser mesh from which this mesh is generated, it equals NULL if _level = 0 */
    Mesh* _coarseMsh;

    /** The projection matrix between Lagrange FEM at the same level mesh */
    SparseMatrix* _ProjQitoQj[3][3];

    /** The coarse to the fine projection matrix */
    SparseMatrix* _ProjCoarseToFine[5];

    /** Build the projection matrix between Lagrange FEM at the same level mesh*/
    void BuildQitoQjProjection(const unsigned& itype, const unsigned& jtype);

    /** Build the coarse to the fine projection matrix */
    void BuildCoarseToFineProjection(const unsigned& solType);

    //member-data
    int _nelem;                                //< number of elements
    unsigned _nnodes;                          //< number of nodes
    unsigned _level;                           //< level of mesh in the multilevel hierarchy
    static unsigned _dimension;                //< dimension of the problem
    static unsigned _ref_index;
    static unsigned _face_index;

    std::map < unsigned, unsigned > _ownedGhostMap[2];
    vector < unsigned > _originalOwnSize[2];

    static const unsigned _END_IND[5];
    vector < vector < double > > _coords;

    // indices of the topology parallel vectors
    static const unsigned _xIndex = 0;
    static const unsigned _yIndex = 1;
    static const unsigned _zIndex = 2;
    static const unsigned _amrIndex = 3;
    static const unsigned _materialIndex = 4;
    static const unsigned _groupIndex = 5;
    static const unsigned _typeIndex = 6;


};

} //end namespace femus



#endif

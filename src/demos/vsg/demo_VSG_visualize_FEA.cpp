// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2023 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban
// =============================================================================
//
// FEA visualization using Chrono::VSG
//
// =============================================================================

#include "chrono/physics/ChSystemSMC.h"
#include "chrono/solver/ChIterativeSolverLS.h"

#include "chrono/fea/ChElementSpring.h"
#include "chrono/fea/ChElementBar.h"
#include "chrono/fea/ChElementTetraCorot_4.h"
#include "chrono/fea/ChElementTetraCorot_10.h"
#include "chrono/fea/ChElementHexaCorot_8.h"
#include "chrono/fea/ChElementHexaCorot_20.h"
#include "chrono/fea/ChMesh.h"
#include "chrono/fea/ChMeshFileLoader.h"
#include "chrono/fea/ChLinkPointFrame.h"
#include "chrono/assets/ChVisualShapeFEA.h"

#include "chrono_vsg/ChVisualSystemVSG.h"

using namespace chrono;
using namespace chrono::fea;
using namespace chrono::vsg3d;

int main(int argc, char* argv[]) {
    GetLog() << "Copyright (c) 2017 projectchrono.org\nChrono version: " << CHRONO_VERSION << "\n\n";

    // Create a Chrono system
    ChSystemSMC sys;

    // Create a mesh, a container for groups of elements and their referenced nodes
    auto my_mesh = chrono_types::make_shared<ChMesh>();

    // Create a material, assigned to each element, and set its parameters
    auto mmaterial = chrono_types::make_shared<ChContinuumElastic>();
    mmaterial->Set_E(0.01e9);
    mmaterial->Set_v(0.3);
    mmaterial->Set_RayleighDampingK(0.001);
    mmaterial->Set_density(1000);

    // Add some TETAHEDRONS from .nmode and .ele input files
    try {
        ChMeshFileLoader::FromTetGenFile(my_mesh, GetChronoDataFile("fea/beam.node").c_str(),
                                         GetChronoDataFile("fea/beam.ele").c_str(), mmaterial);
    } catch (const ChException& myerr) {
        GetLog() << myerr.what();
        return 0;
    }

    // Apply a force to a node
    auto mnodelast = std::dynamic_pointer_cast<ChNodeFEAxyz>(my_mesh->GetNode(my_mesh->GetNnodes() - 1));
    mnodelast->SetForce(ChVector<>(50, 0, 50));

    // Add some HEXAHEDRONS (isoparametric bricks)
    ChVector<> hexpos(0, 0, 0);
    double sx = 0.1;
    double sz = 0.1;
    for (int e = 0; e < 6; ++e) {
        double angle = e * (2 * CH_C_PI / 8.0);
        hexpos.z() = 0.3 * cos(angle);
        hexpos.x() = 0.3 * sin(angle);
        ChMatrix33<> hexrot(Q_from_AngAxis(angle, VECT_Y));

        std::shared_ptr<ChNodeFEAxyz> hnode1_lower;
        std::shared_ptr<ChNodeFEAxyz> hnode2_lower;
        std::shared_ptr<ChNodeFEAxyz> hnode3_lower;
        std::shared_ptr<ChNodeFEAxyz> hnode4_lower;

        for (int ilayer = 0; ilayer < 6; ++ilayer) {
            double hy = ilayer * sz;
            auto hnode1 = chrono_types::make_shared<ChNodeFEAxyz>(hexpos + hexrot * ChVector<>(0, hy, 0));
            auto hnode2 = chrono_types::make_shared<ChNodeFEAxyz>(hexpos + hexrot * ChVector<>(0, hy, sz));
            auto hnode3 = chrono_types::make_shared<ChNodeFEAxyz>(hexpos + hexrot * ChVector<>(sx, hy, sz));
            auto hnode4 = chrono_types::make_shared<ChNodeFEAxyz>(hexpos + hexrot * ChVector<>(sx, hy, 0));
            my_mesh->AddNode(hnode1);
            my_mesh->AddNode(hnode2);
            my_mesh->AddNode(hnode3);
            my_mesh->AddNode(hnode4);

            if (ilayer > 0) {
                auto helement1 = chrono_types::make_shared<ChElementHexaCorot_8>();
                helement1->SetNodes(hnode1_lower, hnode2_lower, hnode3_lower, hnode4_lower, hnode1, hnode2, hnode3,
                                    hnode4);
                helement1->SetMaterial(mmaterial);

                my_mesh->AddElement(helement1);
            }

            hnode1_lower = hnode1;
            hnode2_lower = hnode2;
            hnode3_lower = hnode3;
            hnode4_lower = hnode4;
        }

        // Set an initial displacement to a node
        hnode4_lower->SetPos(hnode4_lower->GetX0() + hexrot * ChVector<>(0.1, 0.1, 0));

        // Apply a force to a node
        hnode4_lower->SetForce(hexrot * ChVector<>(500, 0, 0));
    }

    // Add the mesh to the system
    sys.Add(my_mesh);

    // Create a truss
    auto truss = chrono_types::make_shared<ChBody>();
    truss->SetBodyFixed(true);
    sys.Add(truss);

    // Create constraints between nodes and truss
    // (for example, fix to ground all nodes which are near y=0
    for (unsigned int inode = 0; inode < my_mesh->GetNnodes(); ++inode) {
        if (auto mnode = std::dynamic_pointer_cast<ChNodeFEAxyz>(my_mesh->GetNode(inode))) {
            if (mnode->GetPos().y() < 0.01) {
                auto constraint = chrono_types::make_shared<ChLinkPointFrame>();
                constraint->Initialize(mnode, truss);
                sys.Add(constraint);

                // For example, attach small cube to show the constraint
                auto mboxfloor = chrono_types::make_shared<ChBoxShape>();
                mboxfloor->GetBoxGeometry().Size = ChVector<>(0.005);
                constraint->AddVisualShape(mboxfloor);
            }
        }
    }

    // Visualization of the FEM mesh.
    // This will automatically update a triangle mesh (internally managed) by setting  proper coordinates and vertex colors
    auto mvisualizemesh = chrono_types::make_shared<ChVisualShapeFEA>(my_mesh);
    mvisualizemesh->SetFEMdataType(ChVisualShapeFEA::DataType::NODE_SPEED_NORM);
    mvisualizemesh->SetColorscaleMinMax(0.0, 5.50);
    mvisualizemesh->SetShrinkElements(true, 0.85);
    mvisualizemesh->SetSmoothFaces(true);
    my_mesh->AddVisualShapeFEA(mvisualizemesh);

    auto mvisualizemeshref = chrono_types::make_shared<ChVisualShapeFEA>(my_mesh);
    mvisualizemeshref->SetFEMdataType(ChVisualShapeFEA::DataType::SURFACE);
    mvisualizemeshref->SetWireframe(true);
    mvisualizemeshref->SetDrawInUndeformedReference(true);
    my_mesh->AddVisualShapeFEA(mvisualizemeshref);

    auto mvisualizemeshC = chrono_types::make_shared<ChVisualShapeFEA>(my_mesh);
    mvisualizemeshC->SetFEMglyphType(ChVisualShapeFEA::GlyphType::NODE_DOT_POS);
    mvisualizemeshC->SetFEMdataType(ChVisualShapeFEA::DataType::NONE);
    mvisualizemeshC->SetSymbolsThickness(0.006);
    my_mesh->AddVisualShapeFEA(mvisualizemeshC);

    // Create the visualization system
    ChVisualSystemVSG vis;
    vis.AttachSystem(&sys);
    vis.SetWindowSize(800, 600);
    vis.SetWindowTitle("VSG FEA visualization");
    vis.SetUseSkyBox(true);
    vis.SetLightIntensity(1.0f);
    vis.SetLightDirection(1.5 * CH_C_PI_2, CH_C_PI_4);
    vis.AddCamera(ChVector<>(0.0, 0.6, -1.0));
    vis.Initialize();

    // Solver settings
    sys.SetTimestepperType(chrono::ChTimestepper::Type::EULER_IMPLICIT_LINEARIZED);

    auto solver = chrono_types::make_shared<ChSolverMINRES>();
    sys.SetSolver(solver);
    solver->SetMaxIterations(40);
    solver->SetTolerance(1e-10);
    solver->EnableDiagonalPreconditioner(true);
    solver->EnableWarmStart(true);
    solver->SetVerbose(false);

    // Simulation loop
    while (vis.Run()) {
        vis.Render();
        sys.DoStepDynamics(0.001);
    }

    return 0;
}

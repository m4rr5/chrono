// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Author: Wei Hu, Radu Serban
// =============================================================================

// General Includes
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>

// Chrono includes
#include "chrono/ChConfig.h"
#include "chrono/physics/ChSystemSMC.h"
#include "chrono/physics/ChBodyEasy.h"
#include "chrono/physics/ChInertiaUtils.h"
#include "chrono/physics/ChLinkMotorRotationAngle.h"
#include "chrono/utils/ChUtilsGeometry.h"
#include "chrono/utils/ChUtilsInputOutput.h"
#include "chrono/assets/ChTriangleMeshShape.h"
#include "chrono/core/ChTimer.h"

// Chrono fsi includes
#include "chrono_fsi/ChSystemFsi.h"
#include "chrono_fsi/utils/ChFsiVisualization.h"

#include "chrono_thirdparty/filesystem/path.h"

// Chrono namespaces
using namespace chrono;
using namespace chrono::fsi;
using namespace chrono::geometry;

// Physical properties of terrain particles
double iniSpacing = 0.01;
double kernelLength = 0.01;
double density = 1700.0;

// Dimension of the terrain container
double smalldis = 1.0e-9;
double bxDim = 5.0 + smalldis;
double byDim = 0.8 + smalldis;
double bzDim = 0.12 + smalldis;

// Dimension of the terrain domain
double fxDim = 5.0 + smalldis;
double fyDim = 0.8 + smalldis;
double fzDim = 0.1 + smalldis;

// Terrain region center and its half size
ChVector<> boxCenter(0.0, 0.0, fzDim / 2);
ChVector<> boxHalfDim(fxDim / 2, fyDim / 2, fzDim / 2);

// Size of the wheel
double wheel_radius = 0.47;
double wheel_slip = 0.0;
double wheel_AngVel = 1.0;
double total_mass = 105.22;
std::string wheel_obj = "vehicle/hmmwv/hmmwv_tire_coarse_closed.obj";

// Initial Position of wheel
ChVector<> wheel_IniPos(-bxDim / 2 + wheel_radius, 0.0, 0.0);
ChVector<> wheel_IniVel(0.0, 0.0, 0.0);

// simulation time and stepsize
double total_time = 5.0;
double dT = 2.5e-4;

// linear actuator and angular actuator
auto actuator = chrono_types::make_shared<ChLinkLinActuator>();
auto motor = chrono_types::make_shared<ChLinkMotorRotationAngle>();

// Save data as csv files to see the results off-line using Paraview
bool output = true;
int out_fps = 20;

// Output directories and settings
std::string demo_dir;
const std::string out_dir = GetChronoOutputPath() + "FSI_Single_Wheel_Test/";

// Enable/disable run-time visualization (if Chrono::OpenGL is available)
bool render = true;
float render_fps = 100;

// Verbose temrinal output
bool verbose = true;

//------------------------------------------------------------------
// Function to save wheel to Paraview VTK files
//------------------------------------------------------------------
void WritewheelVTK(ChSystemSMC& sysMBS, int this_frame) {
    auto body = sysMBS.Get_bodylist()[1];
    ChFrame<> body_ref_frame = body->GetFrame_REF_to_abs();
    ChVector<> body_pos = body_ref_frame.GetPos();
    ChQuaternion<> body_rot = body_ref_frame.GetRot();

    auto mmesh = chrono_types::make_shared<ChTriangleMeshConnected>();
    double scale_ratio = 1.0;
    mmesh->LoadWavefrontMesh(GetChronoDataFile(wheel_obj), false, true);
    mmesh->Transform(ChVector<>(0, 0, 0), ChMatrix33<>(scale_ratio));  // scale to a different size
    mmesh->RepairDuplicateVertexes(1e-9);                              // if meshes are not watertight

    double mmass;
    ChVector<> mcog;
    ChMatrix33<> minertia;
    mmesh->ComputeMassProperties(true, mmass, mcog, minertia);
    mmesh->Transform(body_pos, ChMatrix33<>(body_rot));  // rotate the mesh based on the orientation of body

    // char filename[4096];
    /*if(1==0){// save to obj file
        sprintf(filename, "%s/hmmwv_tire_%d.obj", paramsH->demo_dir, this_frame);
        std::vector<geometry::ChTriangleMeshConnected> meshes = { *mmesh };
        geometry::ChTriangleMeshConnected::WriteWavefront(filename, meshes);
    }
    if(1==1){// save to vtk file
        sprintf(filename, "%s/hmmwv_tire_%d.vtk", paramsH->demo_dir, this_frame);
        std::ofstream file;
        file.open(filename);
        file << "# vtk DataFile Version 2.0" << std::endl;
        file << "VTK from simulation" << std::endl;
        file << "ASCII" << std::endl;
        file << "DATASET UNSTRUCTURED_GRID" << std::endl;
        int nv = mmesh->getCoordsVertices().size();
        file << "POINTS " << nv << " " << "float" << std::endl;
        for (auto& v : mmesh->getCoordsVertices()) {
            file << v.x() << " " << v.y() << " " << v.z() << std::endl;
        }
        int nf = mmesh->getIndicesVertexes().size();
        file << "CELLS " << nf << " " << 4*nf << std::endl;
        for (auto& f : mmesh->getIndicesVertexes()) {
            file <<  "3 " << f.x()  << " " << f.y() << " " << f.z()  << std::endl;
        }
        file << "CELL_TYPES " << nf << std::endl;
        for (auto& f : mmesh->getIndicesVertexes()) {
            file <<  "5 " << std::endl;
        }
        file.close();
    }*/
}

//------------------------------------------------------------------
// Function to save the paraview files
//------------------------------------------------------------------
void SaveParaViewFiles(ChSystemFsi& sysFSI,
                       ChSystemSMC& sysMBS,
                       int this_frame,
                       double mTime,
                       std::shared_ptr<ChBody> wheel) {
    // save particles to cvs files
    sysFSI.PrintParticleToFile(demo_dir);

    // save rigid bodies to vtk files
    char SaveAsRigidObjVTK[256];
    static int RigidCounter = 0;
    snprintf(SaveAsRigidObjVTK, sizeof(char) * 256, (demo_dir + "/wheel.%d.vtk").c_str(), RigidCounter);
    WritewheelVTK(sysMBS, this_frame);
    RigidCounter++;
    if (verbose) {
        std::cout << "\n--------------------------------\n" << std::endl;
        std::cout << "------------ Output Frame:   " << this_frame << std::endl;
        std::cout << "------------ Sim Time:       " << mTime << " (s)\n" << std::endl;
        std::cout << "--------------------------------\n" << std::endl;
    }
}

//------------------------------------------------------------------
// Function to creat BCE particles from a mesh
//------------------------------------------------------------------
void CreateMeshMarkers(std::shared_ptr<geometry::ChTriangleMeshConnected> mesh,
                       double delta,
                       std::vector<ChVector<>>& point_cloud) {
    mesh->RepairDuplicateVertexes(1e-9);  // if meshes are not watertight

    ChVector<> minV = mesh->m_vertices[0];
    ChVector<> maxV = mesh->m_vertices[0];
    ChVector<> currV = mesh->m_vertices[0];
    for (unsigned int i = 1; i < mesh->m_vertices.size(); ++i) {
        currV = mesh->m_vertices[i];
        if (minV.x() > currV.x())
            minV.x() = currV.x();
        if (minV.y() > currV.y())
            minV.y() = currV.y();
        if (minV.z() > currV.z())
            minV.z() = currV.z();
        if (maxV.x() < currV.x())
            maxV.x() = currV.x();
        if (maxV.y() < currV.y())
            maxV.y() = currV.y();
        if (maxV.z() < currV.z())
            maxV.z() = currV.z();
    }

    const double EPSI = 1e-6;

    ChVector<> ray_origin;
    for (double x = minV.x(); x < maxV.x(); x += delta) {
        ray_origin.x() = x + 1e-9;
        for (double y = minV.y(); y < maxV.y(); y += delta) {
            ray_origin.y() = y + 1e-9;
            for (double z = minV.z(); z < maxV.z(); z += delta) {
                ray_origin.z() = z + 1e-9;

                ChVector<> ray_dir[2] = {ChVector<>(5, 0.5, 0.25), ChVector<>(-3, 0.7, 10)};
                int intersectCounter[2] = {0, 0};

                for (unsigned int i = 0; i < mesh->m_face_v_indices.size(); ++i) {
                    auto& t_face = mesh->m_face_v_indices[i];
                    auto& v1 = mesh->m_vertices[t_face.x()];
                    auto& v2 = mesh->m_vertices[t_face.y()];
                    auto& v3 = mesh->m_vertices[t_face.z()];

                    /// Find vectors for two edges sharing V1
                    auto edge1 = v2 - v1;
                    auto edge2 = v3 - v1;

                    bool t_inter[2] = {false, false};

                    for (unsigned int j = 0; j < 2; j++) {
                        /// Begin calculating determinant - also used to calculate uu parameter
                        auto pvec = Vcross(ray_dir[j], edge2);
                        /// if determinant is near zero, ray is parallel to plane of triangle
                        double det = Vdot(edge1, pvec);
                        /// NOT CULLING
                        if (det > -EPSI && det < EPSI) {
                            t_inter[j] = false;
                            continue;
                        }
                        double inv_det = 1.0 / det;

                        /// calculate distance from V1 to ray origin
                        auto tvec = ray_origin - v1;

                        /// Calculate uu parameter and test bound
                        double uu = Vdot(tvec, pvec) * inv_det;
                        /// The intersection lies outside of the triangle
                        if (uu < 0.0 || uu > 1.0) {
                            t_inter[j] = false;
                            continue;
                        }

                        /// Prepare to test vv parameter
                        auto qvec = Vcross(tvec, edge1);

                        /// Calculate vv parameter and test bound
                        double vv = Vdot(ray_dir[j], qvec) * inv_det;
                        /// The intersection lies outside of the triangle
                        if (vv < 0.0 || ((uu + vv) > 1.0)) {
                            t_inter[j] = false;
                            continue;
                        }

                        double tt = Vdot(edge2, qvec) * inv_det;
                        if (tt > EPSI) {  /// ray intersection
                            t_inter[j] = true;
                            continue;
                        }

                        /// No hit, no win
                        t_inter[j] = false;
                    }

                    intersectCounter[0] += t_inter[0] ? 1 : 0;
                    intersectCounter[1] += t_inter[1] ? 1 : 0;
                }

                if (((intersectCounter[0] % 2) == 1) && ((intersectCounter[1] % 2) == 1))  // inside mesh
                    point_cloud.push_back(ChVector<>(x, y, z));
            }
        }
    }
}

//------------------------------------------------------------------
// Create the objects of the MBD system. Rigid bodies, and if FSI,
// their BCE representation are created and added to the systems
//------------------------------------------------------------------
void CreateSolidPhase(ChSystemSMC& sysMBS, ChSystemFsi& sysFSI) {
    /// Set common material Properties
    auto mysurfmaterial = chrono_types::make_shared<ChMaterialSurfaceSMC>();
    mysurfmaterial->SetYoungModulus(1e8);
    mysurfmaterial->SetFriction(0.9f);
    mysurfmaterial->SetRestitution(0.4f);
    mysurfmaterial->SetAdhesion(0);

    // -----------------------------------------------------
    // Create a container -- always FIRST body in the system
    // -----------------------------------------------------
    auto ground = chrono_types::make_shared<ChBodyEasyBox>(100, 100, 0.02, 1000, false, true, mysurfmaterial);
    ground->SetPos(ChVector<>(0.0, 0.0, 0.0));
    ground->SetCollide(true);
    ground->SetBodyFixed(true);
    sysMBS.AddBody(ground);

    /// Bottom wall
    ChVector<> size_XY(bxDim / 2 + 3 * iniSpacing, byDim / 2 + 0 * iniSpacing, 2 * iniSpacing);
    ChVector<> pos_zn(0, 0, -3 * iniSpacing);
    ChVector<> pos_zp(0, 0, bzDim + 2 * iniSpacing);

    /// left and right Wall
    ChVector<> size_YZ(2 * iniSpacing, byDim / 2 + 0 * iniSpacing, bzDim / 2);
    ChVector<> pos_xp(bxDim / 2 + iniSpacing, 0.0, bzDim / 2 + 0 * iniSpacing);
    ChVector<> pos_xn(-bxDim / 2 - 3 * iniSpacing, 0.0, bzDim / 2 + 0 * iniSpacing);

    /// Front and back Wall
    ChVector<> size_XZ(bxDim / 2 + 3 * iniSpacing, 2 * iniSpacing, bzDim / 2);
    ChVector<> pos_yp(0, byDim / 2 + iniSpacing, bzDim / 2 + 0 * iniSpacing);
    ChVector<> pos_yn(0, -byDim / 2 - 3 * iniSpacing, bzDim / 2 + 0 * iniSpacing);

    /// Add BCE particles attached on the walls into FSI system
    /// sysFSI.AddBceBox(ground, pos_zp, QUNIT, size_XY, 12);
    sysFSI.AddBceBox(ground, pos_zn, QUNIT, size_XY, 12);
    sysFSI.AddBceBox(ground, pos_xp, QUNIT, size_YZ, 23);
    sysFSI.AddBceBox(ground, pos_xn, QUNIT, size_YZ, 23);
    /// sysFSI.AddBceBox(ground, pos_yp, QUNIT, size_XZ, 13);
    /// sysFSI.AddBceBox(ground, pos_yn, QUNIT, size_XZ, 13);

    // -----------------------------------------------------
    // Create the wheel -- always SECOND body in the system
    // -----------------------------------------------------
    /// load mesh from obj file
    auto mmesh = chrono_types::make_shared<ChTriangleMeshConnected>();
    double scale_ratio = 1.0;
    mmesh->LoadWavefrontMesh(GetChronoDataFile(wheel_obj), false, true);
    // mmesh->Transform(ChVector<>(0, 0, 0), ChMatrix33<>(body_rot));       // rotate the mesh if needed
    mmesh->Transform(ChVector<>(0, 0, 0), ChMatrix33<>(scale_ratio));  // scale to a different size
    mmesh->RepairDuplicateVertexes(1e-9);                              // if meshes are not watertight

    /// compute mass inertia from mesh
    double mmass;
    double mdensity = 1500.0;
    ChVector<> mcog;
    ChMatrix33<> minertia;
    mmesh->ComputeMassProperties(true, mmass, mcog, minertia);
    ChMatrix33<> principal_inertia_rot;
    ChVector<> principal_I;
    ChInertiaUtils::PrincipalInertia(minertia, principal_I, principal_inertia_rot);
    mcog = ChVector<>(0.0, 0.0, 0.0);

    /// set the abs orientation, position and velocity
    auto wheel = chrono_types::make_shared<ChBodyAuxRef>();
    ChQuaternion<> Body_rot = Q_from_Euler123(ChVector<double>(0, 0, 0));
    ChVector<> Body_pos = wheel_IniPos + ChVector<>(0, 0, wheel_radius + fzDim);
    ChVector<> Body_vel = wheel_IniVel;

    /// Set the COG coordinates to barycenter, without displacing the REF reference.
    /// Make the COG frame a principal frame.
    wheel->SetFrame_COG_to_REF(ChFrame<>(mcog, principal_inertia_rot));

    /// Set inertia
    wheel->SetMass(total_mass * 1.0 / 2.0);
    wheel->SetInertiaXX(mdensity * principal_I);
    wheel->SetPos_dt(Body_vel);
    wheel->SetWvel_loc(ChVector<>(0.0, 0.0, 0.0));  // set an initial anular velocity (rad/s)

    /// Set the absolute position of the body:
    wheel->SetFrame_REF_to_abs(ChFrame<>(ChVector<>(Body_pos), ChQuaternion<>(Body_rot)));
    sysMBS.AddBody(wheel);

    wheel->SetBodyFixed(false);
    wheel->GetCollisionModel()->ClearModel();
    wheel->GetCollisionModel()->AddTriangleMesh(mysurfmaterial, mmesh, false, false, VNULL, ChMatrix33<>(1), 0.005);
    wheel->GetCollisionModel()->BuildModel();
    wheel->SetCollide(false);

    /// Add this body to the FSI system
    std::vector<ChVector<>> BCE_par_rock;
    CreateMeshMarkers(mmesh, iniSpacing, BCE_par_rock);
    sysFSI.AddBceFromPoints(wheel, BCE_par_rock, ChVector<>(0.0), QUNIT);
    sysFSI.AddFsiBody(wheel);

    // -----------------------------------------------------
    // Create the chassis -- always THIRD body in the system
    // -----------------------------------------------------
    // Initially, the chassis is fixed to ground.
    // It is released after the settling phase.
    auto chassis = chrono_types::make_shared<ChBody>();
    // chassis->SetIdentifier(Id_chassis);
    chassis->SetMass(total_mass * 1.0 / 2.0);
    chassis->SetPos(wheel->GetPos());
    chassis->SetCollide(false);
    chassis->SetBodyFixed(false);

    // Add geometry of the chassis.
    chassis->GetCollisionModel()->ClearModel();
    chrono::utils::AddBoxGeometry(chassis.get(), mysurfmaterial, ChVector<>(0.1, 0.1, 0.1), ChVector<>(0, 0, 0));
    chassis->GetCollisionModel()->BuildModel();
    sysMBS.AddBody(chassis);

    // ---------------------------------------------------
    // Create the axle -- always FOURTH body in the system
    // ---------------------------------------------------
    auto axle = chrono_types::make_shared<ChBody>();
    // axle->SetIdentifier(Id_axle);
    axle->SetMass(total_mass * 1.0 / 2.0);
    axle->SetPos(wheel->GetPos());
    axle->SetCollide(false);
    axle->SetBodyFixed(false);
    /// Add geometry of the axle.
    axle->GetCollisionModel()->ClearModel();
    chrono::utils::AddSphereGeometry(axle.get(), mysurfmaterial, 0.5, ChVector<>(0, 0, 0));
    axle->GetCollisionModel()->BuildModel();
    sysMBS.AddBody(axle);

    // =============================================================================
    // Connect the chassis to the containing bin (ground) through a translational
    // joint and create a linear actuator.
    // =============================================================================
    auto prismatic1 = chrono_types::make_shared<ChLinkLockPrismatic>();
    prismatic1->Initialize(ground, chassis, ChCoordsys<>(chassis->GetPos(), Q_from_AngY(CH_C_PI_2)));
    prismatic1->SetName("prismatic_chassis_ground");
    sysMBS.AddLink(prismatic1);

    double velocity = wheel_AngVel * wheel_radius * (1.0 - wheel_slip);
    auto actuator_fun = chrono_types::make_shared<ChFunction_Ramp>(0.0, velocity);

    actuator->Initialize(ground, chassis, false, ChCoordsys<>(chassis->GetPos(), QUNIT),
                         ChCoordsys<>(chassis->GetPos() + ChVector<>(1, 0, 0), QUNIT));
    actuator->SetName("actuator");
    actuator->Set_lin_offset(1);
    actuator->Set_dist_funct(actuator_fun);
    sysMBS.AddLink(actuator);

    // =============================================================================
    // Connect the axle to the chassis through a vertical translational joint.
    // =============================================================================
    auto prismatic2 = chrono_types::make_shared<ChLinkLockPrismatic>();
    prismatic2->Initialize(chassis, axle, ChCoordsys<>(chassis->GetPos(), QUNIT));
    prismatic2->SetName("prismatic_axle_chassis");
    sysMBS.AddLink(prismatic2);

    // =============================================================================
    // Connect the wheel to the axle through a engine joint.
    // =============================================================================
    motor->SetName("engine_wheel_axle");
    motor->Initialize(wheel, axle,
                      ChFrame<>(wheel->GetPos(), chrono::Q_from_AngAxis(-CH_C_PI / 2.0, ChVector<>(1, 0, 0))));
    motor->SetAngleFunction(chrono_types::make_shared<ChFunction_Ramp>(0, wheel_AngVel));
    sysMBS.AddLink(motor);
}

// =============================================================================

int main(int argc, char* argv[]) {
    // Create the MBS and FSI systems
    ChSystemSMC sysMBS;
    ChSystemFsi sysFSI(sysMBS);

    ChVector<> gravity = ChVector<>(0, 0, -9.81);
    sysMBS.Set_G_acc(gravity);
    sysFSI.Set_G_acc(gravity);

    // Use the default input file or you may enter your input parameters as a command line argument
    std::string inputJson = GetChronoDataFile("fsi/input_json/demo_FSI_SingleWheelTest.json");
    if (argc == 3) {
        std::string my_inputJson = std::string(argv[1]);
        inputJson = my_inputJson;
        wheel_slip = std::stod(argv[2]);
    } else if (argc != 1) {
        std::cout << "usage: ./demo_FSI_SingleWheelTest <json_file> <wheel_slip>" << std::endl;
        std::cout << "or to use default input parameters ./demo_FSI_SingleWheelTest " << std::endl;
        return 1;
    }
    std::cout << "Using JSON file: " << inputJson << std::endl;
    sysFSI.SetSimParameter(inputJson, ChVector<>(bxDim, byDim, bzDim));

    sysFSI.SetInitialSpacing(iniSpacing);
    sysFSI.SetKernelLength(kernelLength);
    sysFSI.SetStepSize(dT);

    // Set the terrain size
    sysFSI.SetSimDim(ChVector<>(fxDim, fyDim, fzDim));

    // Set the terrain container size
    sysFSI.SetContainerDim(ChVector<>(bxDim, byDim, bzDim));

    // Set SPH discretization type, consistent or inconsistent
    sysFSI.SetDiscreType(false, false);

    // Set wall boundary condition
    sysFSI.SetWallBC(BceVersion::ORIGINAL);

    // Setup the solver based on the input value of the prameters
    sysFSI.SetFluidDynamics(fluid_dynamics::WCSPH);

    // Set up the periodic boundary condition (if not, set relative larger values)
    ChVector<> cMin(-bxDim / 2 * 10, -byDim / 2 - 0.5 * iniSpacing, -bzDim * 10 - 10 * iniSpacing);
    ChVector<> cMax(bxDim / 2 * 10, byDim / 2 + 0.5 * iniSpacing, bzDim * 10 + 10 * iniSpacing);
    sysFSI.SetBoundaries(cMin, cMax);

    // Setup sub domains for faster neighbor particle search
    //// RADU TODO
    //// Remove as a user option - ALWAYS use subdomains
    sysFSI.SetSubdomains(true);

    /// Setup the output directory for FSI data
    sysFSI.SetFsiOutputDir(demo_dir, out_dir, inputJson);

    // Set FSI information output
    sysFSI.SetFsiInfoOutput(false);

    // Set simulation data output length
    sysFSI.SetOutputLength(0);

    sysFSI.AddSphMarkerBox(iniSpacing, kernelLength, boxCenter, boxHalfDim);

    // Create Solid region and attach BCE SPH particles
    CreateSolidPhase(sysMBS, sysFSI);

    // Construction of the FSI system must be finalized before running
    sysFSI.Finalize();

    // Set up integrator for the multi-body dynamics system
    sysMBS.SetTimestepperType(ChTimestepper::Type::HHT);
    auto mystepper = std::static_pointer_cast<ChTimestepperHHT>(sysMBS.GetTimestepper());
    mystepper->SetAlpha(-0.2);
    mystepper->SetMaxiters(1000);
    mystepper->SetAbsTolerances(1e-6);
    mystepper->SetMode(ChTimestepperHHT::ACCELERATION);
    mystepper->SetScaling(true);

    auto wheel = sysMBS.Get_bodylist()[1];
    ChVector<> force = actuator->Get_react_force();
    ChVector<> torque = motor->Get_react_torque();
    ChVector<> w_pos = wheel->GetPos();
    ChVector<> w_vel = wheel->GetPos_dt();
    ChVector<> angvel = wheel->GetWvel_loc();

    // Write the information into a txt file
    std::ofstream myFile;
    if (output) {
        myFile.open(demo_dir + "/results.txt", std::ios::trunc);
    }

    // Create a run-tme visualizer
    ChFsiVisualization fsi_vis(&sysFSI);
    if (render) {
        fsi_vis.SetTitle("Chrono::FSI single wheel demo");
        fsi_vis.SetCameraPosition(ChVector<>(0, -5 * byDim, 5 * bzDim), ChVector<>(0, 0, 0));
        fsi_vis.SetCameraMoveScale(1.0f);
        fsi_vis.EnableBoundaryMarkers(true);
        fsi_vis.Initialize();
    }

    // Start the simulation
    unsigned int output_steps = (unsigned int)(1 / (out_fps * dT));
    unsigned int render_steps = (unsigned int)(1 / (render_fps * dT));

    double time = 0.0;
    int current_step = 0;

    ChTimer<> timer;
    timer.start();
    while (time < total_time) {
        // Save data into files
        if (output && current_step % output_steps == 0)
            SaveParaViewFiles(sysFSI, sysMBS, current_step, time, wheel);

        // Render SPH particles
        if (render && current_step % render_steps == 0) {
            if (fsi_vis.Render())
                break;
        }

        // Get the infomation of the wheel
        force = actuator->Get_react_force();
        torque = motor->Get_react_torque();
        w_pos = wheel->GetPos();
        w_vel = wheel->GetPos_dt();
        angvel = wheel->GetWvel_loc();

        if (verbose) {
            std::cout << "time: " << time << std::endl;
            std::cout << "  wheel position:         " << w_pos << std::endl;
            std::cout << "  wheel linear velocity:  " << w_vel << std::endl;
            std::cout << "  wheel angular velocity: " << angvel << std::endl;
            std::cout << "  drawbar pull:           " << force << std::endl;
            std::cout << "  wheel torque:           " << torque << std::endl;
        }

        if (output) {
            myFile << time << "\t" << w_pos.x() << "\t" << w_pos.y() << "\t" << w_pos.z() << "\t" << w_vel.x() << "\t"
                   << w_vel.y() << "\t" << w_vel.z() << "\t" << angvel.x() << "\t" << angvel.y() << "\t" << angvel.z()
                   << "\t" << force.x() << "\t" << force.y() << "\t" << force.z() << "\t" << torque.x() << "\t"
                   << torque.y() << "\t" << torque.z() << "\n";
        }

        // Call the FSI solver
        sysFSI.DoStepDynamics_FSI();
        time += dT;
        current_step++;
    }
    timer.stop();
    std::cout << "\nSimulation time: " << timer() << " seconds\n" << std::endl;

    if (output)
        myFile.close();

    return 0;
}
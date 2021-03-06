/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2019 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    NIImporter_RobocupRescue.cpp
/// @author  Daniel Krajzewicz
/// @author  Jakob Erdmann
/// @author  Michael Behrisch
/// @date    Mon, 14.04.2008
/// @version $Id$
///
// Importer for networks stored in robocup rescue league format
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#include <config.h>
#include <string>
#include <utils/xml/SUMOSAXHandler.h>
#include <utils/common/UtilExceptions.h>
#include <utils/common/StringUtils.h>
#include <utils/common/ToString.h>
#include <utils/common/MsgHandler.h>
#include <netbuild/NBEdge.h>
#include <netbuild/NBEdgeCont.h>
#include <netbuild/NBNode.h>
#include <netbuild/NBNodeCont.h>
#include <netbuild/NBNetBuilder.h>
#include <utils/xml/SUMOXMLDefinitions.h>
#include <utils/geom/GeoConvHelper.h>
#include <utils/geom/GeomConvHelper.h>
#include <utils/options/OptionsCont.h>
#include <utils/common/FileHelpers.h>
#include <utils/xml/XMLSubSys.h>
#include <utils/iodevices/BinaryInputDevice.h>
#include "NILoader.h"
#include "NIImporter_RobocupRescue.h"


// ===========================================================================
// method definitions
// ===========================================================================
// ---------------------------------------------------------------------------
// static methods (interface in this case)
// ---------------------------------------------------------------------------
void
NIImporter_RobocupRescue::loadNetwork(const OptionsCont& oc, NBNetBuilder& nb) {
    // check whether the option is set (properly)
    if (!oc.isSet("robocup-dir")) {
        return;
    }
    // build the handler
    NIImporter_RobocupRescue handler(nb.getNodeCont(), nb.getEdgeCont());
    // parse file(s)
    std::vector<std::string> files = oc.getStringVector("robocup-dir");
    for (std::vector<std::string>::const_iterator file = files.begin(); file != files.end(); ++file) {
        // nodes
        std::string nodesName = (*file) + "/node.bin";
        if (!FileHelpers::isReadable(nodesName)) {
            WRITE_ERROR("Could not open robocup-node-file '" + nodesName + "'.");
            return;
        }
        PROGRESS_BEGIN_MESSAGE("Parsing robocup-nodes from '" + nodesName + "'");
        handler.loadNodes(nodesName);
        PROGRESS_DONE_MESSAGE();
        // edges
        std::string edgesName = (*file) + "/road.bin";
        if (!FileHelpers::isReadable(edgesName)) {
            WRITE_ERROR("Could not open robocup-road-file '" + edgesName + "'.");
            return;
        }
        PROGRESS_BEGIN_MESSAGE("Parsing robocup-roads from '" + edgesName + "'");
        handler.loadEdges(edgesName);
        PROGRESS_DONE_MESSAGE();
    }
}



// ---------------------------------------------------------------------------
// loader methods
// ---------------------------------------------------------------------------
NIImporter_RobocupRescue::NIImporter_RobocupRescue(NBNodeCont& nc, NBEdgeCont& ec)
    : myNodeCont(nc), myEdgeCont(ec) {}


NIImporter_RobocupRescue::~NIImporter_RobocupRescue() {
}


void
NIImporter_RobocupRescue::loadNodes(const std::string& file) {
    BinaryInputDevice dev(file);
    int skip;
    dev >> skip; // the number in 19_s
    dev >> skip; // x-offset in 19_s
    dev >> skip; // y-offset in 19_s
    //
    int noNodes;
    dev >> noNodes;
    WRITE_MESSAGE("Expected node number: " + toString(noNodes));
    do {
        //cout << "  left " << (noNodes) << endl;
        int entrySize, id, posX, posY, numEdges;
        dev >> entrySize;
        entrySize /= 4;
        dev >> id;
        dev >> posX;
        dev >> posY;
        dev >> numEdges;

        std::vector<int> edges;
        for (int j = 0; j < numEdges; ++j) {
            int edge;
            dev >> edge;
            edges.push_back(edge);
        }

        int signal;
        dev >> signal;

        std::vector<int> turns;
        for (int j = 0; j < numEdges; ++j) {
            int turn;
            dev >> turn;
            turns.push_back(turn);
        }

        std::vector<std::pair<int, int> > conns;
        for (int j = 0; j < numEdges; ++j) {
            int connF, connT;
            dev >> connF;
            dev >> connT;
            conns.push_back(std::pair<int, int>(connF, connT));
        }

        std::vector<std::vector<int> > times;
        for (int j = 0; j < numEdges; ++j) {
            int t1, t2, t3;
            dev >> t1;
            dev >> t2;
            dev >> t3;
            std::vector<int> time;
            time.push_back(t1);
            time.push_back(t2);
            time.push_back(t3);
            times.push_back(time);
        }

        Position pos((double)(posX / 1000.), -(double)(posY / 1000.));
        NBNetBuilder::transformCoordinate(pos);
        NBNode* node = new NBNode(toString(id), pos);
        myNodeCont.insert(node);
        --noNodes;
    } while (noNodes != 0);
}


void
NIImporter_RobocupRescue::loadEdges(const std::string& file) {
    BinaryInputDevice dev(file);
    int skip;
    dev >> skip; // the number in 19_s
    dev >> skip; // x-offset in 19_s
    dev >> skip; // y-offset in 19_s
    //
    int noEdges;
    dev >> noEdges;
    std::cout << "Expected edge number: " << noEdges << std::endl;
    do {
        std::cout << "  left " << (noEdges) << std::endl;
        int entrySize, id, begNode, endNode, length, roadKind, carsToHead,
            carsToTail, humansToHead, humansToTail, width, block, repairCost, median,
            linesToHead, linesToTail, widthForWalkers;
        dev >> entrySize >> id >> begNode >> endNode >> length >> roadKind >> carsToHead
            >> carsToTail >> humansToHead >> humansToTail >> width >> block >> repairCost
            >> median >> linesToHead >> linesToTail >> widthForWalkers;
        NBNode* fromNode = myNodeCont.retrieve(toString(begNode));
        NBNode* toNode = myNodeCont.retrieve(toString(endNode));
        double speed = (double)(50. / 3.6);
        int priority = -1;
        LaneSpreadFunction spread = linesToHead > 0 && linesToTail > 0 ? LANESPREAD_RIGHT : LANESPREAD_CENTER;
        if (linesToHead > 0) {
            NBEdge* edge = new NBEdge(toString(id), fromNode, toNode, "", speed, linesToHead, priority, NBEdge::UNSPECIFIED_WIDTH, NBEdge::UNSPECIFIED_OFFSET, "", spread);
            if (!myEdgeCont.insert(edge)) {
                WRITE_ERROR("Could not insert edge '" + toString(id) + "'");
            }
        }
        if (linesToTail > 0) {
            NBEdge* edge = new NBEdge("-" + toString(id), toNode, fromNode, "", speed, linesToTail, priority, NBEdge::UNSPECIFIED_WIDTH, NBEdge::UNSPECIFIED_OFFSET, "", spread);
            if (!myEdgeCont.insert(edge)) {
                WRITE_ERROR("Could not insert edge '-" + toString(id) + "'");
            }
        }
        --noEdges;
    } while (noEdges != 0);
}


/****************************************************************************/


// THE 1982 CRANE - all written material: documents, tapes, endings
#include "game.h"

const char* item_name(int id) {
    switch (id) {
    case IT_FLASHLIGHT: return "Flashlight";
    case IT_RADIO: return "Radio handset";
    case IT_OFFICE_KEY: return "Site office key";
    case IT_FUSE_A: case IT_FUSE_B: case IT_FUSE_C: return "30 A lighting fuse";
    case IT_JERRYCAN: return "Jerry can";
    case IT_CUTTERS: return "Bolt cutters";
    case IT_CRANE_KEY: return "Crane 7 access key";
    case IT_TAPE1: case IT_TAPE2: case IT_TAPE3: case IT_TAPE4: case IT_TAPE5: case IT_TAPE6: return "Cassette tape";
    default: return "Batteries";
    }
}

const char* note_title(int id) {
    switch (id) {
    case N_DUTY_LOG: return "Gate log";
    case N_OFFICE_DOOR: return "Note by the office door";
    case N_INCIDENT: return "Incident report (not filed)";
    case N_TIMECARD: return "Time card - W. Kessler";
    case N_FOREMAN: return "Foreman's day book";
    case N_BLUEPRINT: return "Site plan, Rev. C";
    case N_NEWSPAPER: return "Harbor Gazette";
    case N_CAMP: return "Rules (stores container)";
    case N_DIVER: return "Dive log - Barge H-12";
    case N_RELIEF_LOCKER: return "Locker marked RELIEF";
    case N_WALT_FINAL: return "Cab log - W. Kessler";
    case N_GEN_MANUAL: return "Generator starting procedure";
    case N_LIFTS: return "Lift log - Crane 7";
    case N_PLATE: return "Maker's plate";
    case N_PHOTO: return "Crew photograph";
    case N_MAST: return "A paper napkin";
    default: return "";
    }
}

std::string note_text(const Game& g, int id) {
    switch (id) {
    case N_DUTY_LOG:
        return "HARROW MARINE SALVAGE - GATE LOG\n"
               "Berth 9, Port of Ashgrove\n\n"
               "MON 11/8  06:02  Day crew in (7). Crane on the west quay. Nobody recalls it being rigged. Dale says it has always been there. It is not on the plan.\n\n"
               "MON 11/8  17:40  LIFT 1 logged. Nobody ordered a lift.\n\n"
               "TUE 11/9  06:00  Day crew in (6). Moss did not sign out last night.\n\n"
               "TUE 11/9  23:10  LIFT 2. Kessler in the cab. Says he is only watching it work.\n\n"
               "WED 11/10 06:05  Day crew in (4).\n\n"
               "WED 11/10 22:55  LIFT 3. It is a door. Just a door.\n\n"
               "THU 11/11 06:00  Day crew in (1). Kessler still up top. Will not answer the radio.\n\n"
               "THU 11/11 19:30  Gate power failed. Use the manual release, north wall of the booth.\n\n"
               "THU 11/11 21:15  Called Local 114 for a relief mechanic.\n\n"
               "THU 11/11 23:47  Relief mech. signed in.\n\n\n"
               "The last line is already there. The ink is dry.\n"
               "It is in your handwriting.";
    case N_OFFICE_DOOR:
        return "(taped to the wall in block capitals)\n\n"
               "OFFICE LOCKED. GONE TO FIND TERRY.\n\n"
               "KEY IS IN MY LOCKER IN THE CREW ROOM - GREEN BUILDING, UP THE ROAD ON YOUR RIGHT.\n\n"
               "IF YOU ARE THE RELIEF: DON'T WAIT FOR US.\n\n"
               "AND DON'T LOOK UP AT IT. IT KNOWS WHEN YOU DO.\n\n"
               "- DALE";
    case N_INCIDENT:
        return "INCIDENT REPORT - DRAFT - NOT FILED\n"
               "Foreman: D. Pruett          Date: 11/11/82\n\n"
               "Nature of incident:  I don't know what to put.\n\n"
               "1. Monday morning there was a tower crane on the west quay rails, numbered 7. We have six cranes. The harbor board says we have six. Its plate says 1982 and nothing else.\n\n"
               "2. It has made one lift every night since, from the harbor bed, with nobody rigging the load. After each lift a man has not come in the next morning.\n\n"
               "3. Walt Kessler went up on Tuesday to shut it down. He has not come down. The cab light is on. He waves sometimes. I do not think it is him waving.\n\n"
               "4. Tonight's lift is still on the hook. It is a container. Something inside it keeps time with the knocking in the stacks.\n\n"
               "ACTION TAKEN: Yard lighting is OFF at the generator. I pulled the three fuses and hid them - the stores container in the stacks, the barge, the shop shelves - so nobody would be tempted to light the place up. It moves less in the dark.\n\n"
               "IF SOMEONE MUST GO UP: the crane key is in the key cabinet behind this desk. The lock is electric, so you will need the generator after all. Walt's badge number opens it. Lock the slew before you climb or it will shake you off the mast.";
    case N_TIMECARD:
        return strf("TIME CARD - WEEK ENDING 11/14/82\n\n"
                    "KESSLER, WALTER J.\n"
                    "CRANE OPERATOR          BADGE No. %04d\n\n"
                    "MON   IN 05:58    OUT 18:02\n"
                    "TUE   IN 05:57    OUT --:--\n"
                    "WED   IN --:--    OUT --:--\n"
                    "THU   IN --:--    OUT --:--\n\n"
                    "The punch clock has stamped the margin over and over, every stamp the same:\n\n"
                    "NOV 11 1982  23:47\n"
                    "NOV 11 1982  23:47\n"
                    "NOV 11 1982  23:47\n\n"
                    "The slot beside it holds a fresh card. Your name is already typed on it.",
                    g.keyCode);
    case N_FOREMAN:
        return "FOREMAN'S DAY BOOK - D. PRUETT\n\n"
               "SLEW LOCK, CRANE 7 - for whoever comes after me:\n\n"
               "- The lock lever is on the south-east leg. It needs brake pressure.\n\n"
               "- Pressure comes from the pump in Pump House 2, east end of the quay. Walt hung his lockout on the starter on Monday. He said she doesn't like to be held. Cut it off. Bolt cutters are on the bench in the shop.\n\n"
               "- The pump house floods. PURGE IT BEFORE YOU START THE PUMP or you will be standing in the circuit. The purge order is\n\n"
               "          C  -  A  -  B\n\n"
               "Get it wrong and the relief valve trips and you start again.\n\n"
               "- Then throw the starter. Then pull the lock. Then go up, if you have to.\n\n\n"
               "Thurs. Terry says it is lifting things that were lost. I asked, lost by who. He said it isn't finished.\n\n"
               "Thurs. late. Counted the crew photograph. Seven of us. I can only remember five names.";
    case N_BLUEPRINT:
        return "BERTH 9 SITE PLAN - REV. C - AUGUST 1982\n\n"
               "Stacks A to E. Office. Shop. Pump House 2. Six crane positions, numbered 1 to 6, all on the east quay.\n\n"
               "There is nothing drawn where Crane 7 stands. The paper there is clean - never erased, never drawn on.\n\n"
               "Someone has circled the gap in red and written:\n"
               "WHERE IS IT ON THE PLAN??\n\n"
               "Under that, in a different hand:\n"
               "it's on the plan now. look again tomorrow.";
    case N_NEWSPAPER:
        return "ASHGROVE HARBOR GAZETTE\n"
               "Thursday, November 11, 1982 - 20 cents\n\n"
               "BERTH 9 CREW STILL MISSING\n"
               "Harbor board: \"No crane on Berth 9\"\n\n"
               "Six men employed by Harrow Marine Salvage have now failed to return home this week. A spokesman for the Harbor Board said yesterday that the yard has no crane on its west quay \"and never has had,\" and that reports from Berth 9 are being treated as \"a records problem.\"\n\n"
               "A seventh man, a relief mechanic sent from Local 114 on Thursday night, is also unaccounted for.\n\n\n"
               "The paper is dated today. The ink comes off on your fingers.\n"
               "You only arrived a few minutes ago.";
    case N_CAMP:
        return "(pencil, on the back of a cargo manifest)\n\n"
               "We are sleeping in the stores box now. It can't see in. Rules so far:\n\n"
               "1. Don't look up at the cab. He waves. Don't wave back.\n\n"
               "2. Lights off. It hunts by the lamps. In the dark it only listens - walk soft, stay low, don't run.\n\n"
               "3. If it comes down, get in a locker and stay there. It does not open doors it didn't see you close.\n\n"
               "4. It won't step onto the barge. Something about the water.\n\n"
               "5. If the box in the stacks knocks, don't knock back. Moss knocked back.\n\n"
               "Ruiz says a relief man is coming from the hall. Ruiz says that every night. We never see him arrive. We only ever find the gate open.";
    case N_DIVER:
        return "DIVE LOG - BARGE H-12 - A. ANAND\n\n"
               "11/9  Sent down to see what it is lifting from. 14 fathoms, no visibility. Followed the hoist wire down.\n\n"
               "There is no wreck. There is a floor. Flat, like a quay. Rails on it. I walked the rail with my hand until I found the bogies.\n\n"
               "There is another crane down there. Same as ours. The wire from our hook goes down to it and the wire from its hook comes up to us.\n\n"
               "Its cab light was on.\n\n"
               "11/10  Okoro won't let me dive again. He says my helmet came up ten minutes before I did.\n\n"
               "11/11  If we put the load back, maybe it lets go. PUT IT BACK IN THE WATER. Don't land it on our quay. Whatever is in the box wants a quay.\n\n"
               "The fuse Dale gave me to hide is on the table.";
    case N_RELIEF_LOCKER:
        return "A locker with fresh masking tape across the name plate: RELIEF.\n\n"
               "Inside: a wet oilskin, your size. A Local 114 union card with tonight's date. A flashlight exactly like yours, its batteries dead.\n\n"
               "Scratched into the inside of the door, in neat rows, a tally. Hundreds of marks.\n\n"
               "The newest one is still bright metal.";
    case N_WALT_FINAL:
        return "W. KESSLER - CAB LOG - CRANE 7\n\n"
               "I thought I would shut her down from up here. There is no shutting down. There are three levers and there is the lift.\n\n"
               "I have watched four nights. It brings up things that went under, and every thing it lands on the quay stays, and for every thing that stays a man goes missing to make the room. That is the trade. It is building the yard it remembers.\n\n"
               "Tonight's box has someone in it. I can hear him. He knocks in threes. He is asking to be set down.\n\n"
               "Listen to me, relief - I know it's you, it is always you. You have the levers now. There are two things you can do.\n\n"
               "SLEW HER OUT OVER THE HARBOR, RUN THE TROLLEY OUT, AND LOWER UNTIL THE WATER TAKES IT. PUT IT BACK. THEN RELEASE.\n\n"
               "Or land it on the hatch marks on the quay, and climb down, and open it, and see who it is.\n\n"
               "I landed mine.\n\n"
               "Don't be long deciding. It is on the ladder. It is the one who doesn't use the rungs.";
    case N_GEN_MANUAL:
        return "YARD GENERATOR - STARTING PROCEDURE\n\n"
               "1. Check all three 30 A lighting fuses are seated (panel on the north wall).\n\n"
               "2. Check diesel. The tank on the set takes one can. Fill cans at the bulk tank outside - hand pump, west end.\n\n"
               "3. Pull the start lever at the control end. Hold until she catches. She never catches on the first pull.\n\n\n"
               "Written underneath in grease pencil:\n"
               "FUSES ARE OUT ON PURPOSE.  - D.";
    case N_LIFTS:
        return "LIFT LOG - CRANE 7\n"
               "(nobody is signing these. I am just writing them down. - D.P.)\n\n"
               "LIFT 1  Mon 11/8\nA motor car. A sedan. Seats still wet this morning though it has not rained inside it. The odometer moves.\n\n"
               "LIFT 2  Tue 11/9\nA church bell. It hums when you stand near. No church in Ashgrove ever lost one.\n\n"
               "LIFT 3  Wed 11/10\nA door in its frame. Stands up on its own. Locked. Cold comes through the keyhole. IF IT IS EVER OPEN, DO NOT GO THROUGH.\n\n"
               "LIFT 4  Thu 11/11\nA container. Still on the hook. It has not put this one down. I think it is waiting for someone to tell it where.";
    case N_PLATE:
        return "MAKER'S PLATE - brass, riveted to the leg.\n\n"
               "No. 7\n\n"
               "MFR.       (the metal is smooth. It was never stamped.)\n"
               "YEAR       1982\n"
               "SERIAL    (smooth.)\n\n"
               "The rivets are bright. The steel under your hand is cold and faintly wet, and there are barnacles on it higher than any tide comes.";
    case N_PHOTO:
        return "BERTH 9 CREW - SUMMER '82\n\n"
               "Seven men in hard hats on the quay, squinting into the sun. Every face but one has been scratched away with something sharp.\n\n"
               "The one that is left stands in the middle. He is looking at the camera. No - a little above it.\n\n"
               "Behind them, where the quay should be empty, the photograph shows a mast and a jib.\n\n"
               "It was taken in July. The crane came in November.";
    case N_MAST:
        return "(a paper napkin from a lunch pail)\n\n"
               "Marie - if they bring you my pail, I ate the sandwich, it was good.\n\n"
               "I am going to finish the climb now. There are more landings than there were on Tuesday. I count them and I lose count.\n\n"
               "If I look down, the ladder below me is full.\n\n"
               "I am not going to look down again.";
    default:
        return "";
    }
}

const char* tape_title(int t) {
    switch (t) {
    case 0: return "Tape 1 - W.K. - Monday";
    case 1: return "Tape 2 - Ruiz";
    case 2: return "Tape 3 - Anand, on the wire";
    case 3: return "Tape 4 - W.K. - Wednesday";
    case 4: return "Tape 5 - W.K. - the ladder";
    case 5: return "Tape 6 - W.K. - last";
    default: return "";
    }
}

// Lines are separated by '\n'; each becomes one subtitle while the tape plays.
const char* tape_text(int t) {
    switch (t) {
    case 0:
        return "Kessler. Crane log. Monday the eighth.\n"
               "For the record, I did not rig this crane, and nobody I know did.\n"
               "She's warm. The motors are warm, like she's been working all night.\n"
               "There's a lift on the hook already. A car. Seaweed in the grille.\n"
               "I asked Dale who's been driving her. He looked at me like I'd asked who built the sea.";
    case 1:
        return "It's Ruiz. If you find this - the lamps. It comes to the lamps. We pulled the fuses.\n"
               "Terry went for the fence last night. He said the fence was further away than he could walk.\n"
               "You can't leave by the gate once it's seen you. You leave when it lets go.\n"
               "That's what Anand says. Put it back, he says. Give it back and it lets go.";
    case 2:
        return "(breathing, bubbles) ...Anand, on the wire. Fourteen fathoms.\n"
               "There's... hold on. There's a light down here.\n"
               "It's a cab. There's a crane cab down here with the light on.\n"
               "Somebody's sitting in it. He's working the levers. He's looking up.\n"
               "He's lowering something to us.";
    case 3:
        return "Wednesday. I'm in the cab. I am not driving. I want that understood.\n"
               "I put my hands on the levers and they're already moving.\n"
               "Third lift is a door. I watched it come up through the water standing straight.\n"
               "When it landed, somebody on the quay knocked on it. From our side. Polite.\n"
               "I don't know who's left down there to knock.";
    case 4:
        return "I came up on Tuesday. I have been climbing since.\n"
               "I can see the cab. It doesn't get any nearer. It only gets later.\n"
               "There's a fella below me on the ladder. Oilskin. Hard hat.\n"
               "Every time the lightning goes he's a landing closer, and he hasn't moved his hands.\n"
               "I think he's the relief. I think he's been the relief a long time.";
    case 5:
        return "Relief. You made it up. Good. Sit down. It's your shift.\n"
               "Every night she lifts one. Every night I set it on the quay like a good operator.\n"
               "Every night there's one less of us, and one more thing that shouldn't be here. I only just did the sum.\n"
               "Don't land this one. Swing her out over the water and give it back.\n"
               "I couldn't. I wanted to see who was knocking.\n"
               "...It was me. It's always the operator. Go on. It's on the ladder.";
    default:
        return "";
    }
}

static const EndLine END_A[] = {
    {"You drive.", 3.5f},
    {"The gate gets small in the mirror, and then it isn't there.", 5.5f},
    {"On the coast road the radio finds a station. A man reads the shipping forecast, and then the time.", 7.0f},
    {"\"...six forty-one a.m., Friday the twelfth of November.\"", 5.5f},
    {"Friday. You say it out loud. Friday.", 5.0f},
    {"At the union hall they tell you Berth 9 has been closed for years.\nThey tell you there was never a dispatcher on nights.", 8.0f},
    {"They ask where you got the truck.", 5.0f},
    {"The harbor board's map shows six crane positions on the east quay.\nYou look at it for a long time.", 7.5f},
    {"There is nothing drawn on the west quay.\nThe paper there is clean.", 7.0f},
    {"THE 1982 CRANE\n\nYou put it back.", 9.0f},
};
static const EndLine END_B[] = {
    {"The door is not locked. It was never locked from this side.", 5.5f},
    {"Inside there is a chair, a duty log, and a flashlight exactly like yours.", 6.5f},
    {"The log is open at tonight.\n23:47 - Relief mech. signed in.", 6.5f},
    {"Behind you the door closes the way a door closes when someone is being considerate.", 7.0f},
    {"You feel the hook take the weight.", 5.0f},
    {"You knock. Three times. Polite.", 5.5f},
    {"A long way below, a truck pulls up at the gate.", 6.5f},
    {"THE 1982 CRANE\n\nYou landed it.", 9.0f},
};

const EndLine* ending_lines(int ending, int* count) {
    if (ending == 1) { *count = (int)(sizeof(END_A) / sizeof(END_A[0])); return END_A; }
    *count = (int)(sizeof(END_B) / sizeof(END_B[0]));
    return END_B;
}

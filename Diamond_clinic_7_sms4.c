//#define _CRT_SECURE_NO_DEPRECATE   //sms2: sms uses this, Hossein does not
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include "random.c" //sms2: Hossein uses ".c" instead of ".h" here
#include "DiamondClinic_Code_V5_sms_feedback2.h"

FILE* output_file, * clinic_file, * elevator_file, *gen_results, *animation_file ;
int dummy;

/********************************************************************************************
Notes:
Current assumptions (some of which we'll want to improve later):
- elevators move at a constant velocity at all time; we do not model acceleration/deceleration right now. 
********************************************************************************************/

/********************************************************************************************
Notes:
elevators[i].final_destination: This is the mirroring floor of an elevator. i.e, when the elevator reaches to its final destination (and satisfies some other condition too), it changes its direction.
Elevator will not go beyond its final destination. Elevator's final destination gets updated regularly, and it is also used to assign elevators to floors.
********************************************************************************************/
int main()
{   int reps=REPS;
    int total_reps=reps;
    current_rep = -1;
    event next_event;
    person next_in_Line;
    person next_in_elevator;
    statistics* stats;
    stats = (statistics*)malloc(sizeof(statistics) * REPS);

    // This seeds the Mersenne Twister Random Number Generator
    unsigned long long init[4] = { 0x12345ULL, 0x23456ULL, 0x34567ULL, 0x45678ULL }, length = 4;
    init_by_array64(init, length);
    
    //open all output and input files, and read the latter into appropriate data structures 
    //Open_and_Read_Files();

    //Replicates "REPS" independent days of the clinic
    for (repnum = 0; repnum < REPS; repnum++)
    {   
        //printf("Repnum: %d \n", repnum);
        Open_and_Read_Files();
        stats[repnum].current_rep = repnum + 1;
		stats[repnum].total_reps = reps;
        
        Initialize_Rep();  //sets several counters to 0
        initialize_vars(&stats[repnum]);

        //We can load all Lobby arrivals for the day at time 0, as they are offsets from scheduled clinic times.
        Load_Lobby_Arrivals();  //this includes patients, staff, and doctors

        keepgoing = 1; // if the event list is empty, we will make this keepgoing=0.
        while (keepgoing) // && (tot_events < 17494 ) ) //  19332
        {  
            tot_events++;
            next_event = event_head->Event; // obtain next soonest event scheduled on the event calendar
            Remove_Event(&event_head);
            tnow = next_event.time; //update simulation clock

/**************************************************************************************************************************
This part takes care of the events when a person arrives to lobby or leaves the clinic and would like to go to the lobby.
***************************************************************************************************************************/
            if (next_event.event_type == PERSON_ARRIVES_ELEVATOR_HALL)
            {   
                person_current_floor = people[next_event.entity_type][next_event.entity_index].current_floor;
                person_to_floor = people[next_event.entity_type][next_event.entity_index].to_floor;
                person_direction = ( person_to_floor <= person_current_floor ); // If the statement is true, the direction is DOWN, and otherwise it is up. 
                people[next_event.entity_type][next_event.entity_index].direction = person_direction;

                

                //hp: not sure if/where we use these data; perhaps for verification steps?  in which case you might want to have a counter by floor #, not just lobby vs. "all other floors" counter
                if (person_current_floor == LOBBY) 
                    { lobby_arrival++; } 
                else
                    { person_out_clinic++; } // This paremeter counts the total times that a person gets out of clinic.  

                // If there is an elevator on this floor with space available, then load this person on that elevator; otherwise, load the person onto the wait queue for this floor.            
                // Note: this function returns the elevator number (1-number of elevators, not array index)
                elevator_avail = Elevator_Available(elevators, person_current_floor, person_direction); 
                if (elevator_avail) 
                {   
                    //printf("An elevator was available right away at floors \n ");
                    elevator_index = elevator_avail - 1;  //switch to the array index of the elevator

                    
  
                    Load_Person_Elevator( &elevator_head[elevator_index], tnow, next_event.entity_type, next_event.entity_index, person_to_floor, person_direction ); // add people to elevator's 

                    people[next_event.entity_type][next_event.entity_index].elevator_start_travel_time = tnow;

                    people[next_event.entity_type][next_event.entity_index].elevator_ind[repnum] = elevator_index;




                    
                    elevators[elevator_index].num_people++;
                    elevators[elevator_index].final_destination = max( person_to_floor, elevators[elevator_index].final_destination ) * ( person_current_floor == LOBBY ); // If person is not at the Lobby, then they are going to take the elevator to lobby, so this should be 0.
                    //sms: Note: logic in above line will need to change if we ever consider travel between floors (e.g., from an upper floor to a non-Lobby lower floor
                                       
                    if ( elevators[elevator_index].idle == YES) //this is only the case for the first person getting on the idle elevator at this floor.  Note that others may still join while the door is getting closed.  For now we treat it as the elevator does not re-open upon someone else joining; i.e. the door will definitely close in tnow + DOOR_OPEN_TIME 
                    {   //If the elevator is idle, then we need sending 
                        //printf("An idle elevator was available\n");
                        elevators[elevator_index].direction = person_direction;  
                        elevators[elevator_index].elevator_time[person_direction] += DOOR_TIME;  //HP: at some point we should think if we even need to keep track of total up vs. down time.  fine for now
                        Load_Event(&event_head , tnow + DOOR_TIME , SEND_ELEVATOR, ELEVATOR, elevator_index);  //hp: i think you only allow door to remain open for time from first person getting on until that time + door time.  but i think in case when elevator arrives, you keep resetting the door close time?
                        //sms4: In the above line, I only keep the door open from the time first person arrives until the time it gets closed. I consider elevator times at elevator arrivals.
                        elevators[elevator_index].idle = NO; 
                    } 


                    // Capture animation parameters
                    offload = 0, onload = 1;
                    if (person_direction == UP)
                    {fprintf(animation_file, "%.2f \t elevator_load \t%d\t%d\t UP \t NA \t %d\t%d\t%d \n", tnow, person_current_floor, elevator_index, elevators[elevator_index].num_people-1, offload, onload );
                    }
                    if (person_direction == DOWN)
                    {fprintf(animation_file, "%.2f \t elevator_load \t%d\t%d\t DOWN \t NA \t %d\t%d\t%d \n", tnow, person_current_floor, elevator_index, elevators[elevator_index].num_people-1, offload, onload );
                    }                    
                }
                else 
                /* this else condition happens when either there are no idle nor elevators open and moving in same direction as this 
                   arriving person and having capacity to take them.  Therefore this person is going to have to wait for an elevator. */
                {   
                    Load_Person_Queue( &hall_head[person_direction][person_current_floor], tnow , next_event.entity_type , next_event.entity_index ); // add this person to wait list in that floor
                   
                    //stats[repnum].average_clinic_queue_length += (tnow-clinic_queue_length[clinic_num][1]) * clinic_queue_length[clinic_num][0];

                    //[person_current_floor] += (tnow - queue_registeration_time[person_current_floor])*(queue_size[UP][person_current_floor] + queue_size[DOWN][person_current_floor]);
                    queue_size[person_direction][person_current_floor]++; 
                    stats[repnum].max_hall_queue_size[person_current_floor] = max (queue_size[person_direction][person_current_floor], stats[repnum].max_hall_queue_size[person_current_floor] );
                    //queue_registeration_time[person_current_floor] = tnow;
                    if (queue_size[person_direction][person_current_floor] > stats[repnum].max_hall_queue_size[person_current_floor])
                        { stats[repnum].max_hall_queue_size[person_current_floor] =  queue_size[person_direction][person_current_floor]; }
                    people[next_event.entity_type][next_event.entity_index].line_up_time = tnow;
                    if (queue_size[person_direction][person_current_floor] == 1) // Only the first person requests an elevator. But if there are multiple request pannels at each floor, we need to change this. 
                    {
                        Load_Event( &event_head, tnow+EPSILON , ELEVATOR_REQUEST , next_event.entity_type , next_event.entity_index );       
                    }
                
                    // Capture animation parameters
                    if (person_direction == UP)
                    {fprintf(animation_file, "%.2f \t hall_queue \t%d\t NA\t UP \t NA \t %d\tNA\t NA \n", tnow, person_current_floor, queue_size[person_direction][person_current_floor]);
                    }
                    if (person_direction == DOWN)
                    {fprintf(animation_file, "%.2f \t hall_queue \t%d\t NA\t DOWN \t NA \t %d\tNA\t NA \n", tnow, person_current_floor, queue_size[person_direction][person_current_floor]);
                    }
                }
                
            } //END if (next_event.event_type == PERSON_ARRIVES_ELEVATOR_HALL)

/**************************************************************************************************************************
  This part takes care of sending elevators
**************************************************************************************************************************/
            if (next_event.event_type == SEND_ELEVATOR)
            {//This works as an actuator for sending an elevator, which is getting done floor by floor.
                elevator_index = next_event.entity_index;
                elevator_direction = elevators[elevator_index].direction;
                elevators[elevator_index].idle = NO; // Start moving
                if ( elevator_direction == UP)
                    {elevators[elevator_index].current_floor = elevators[elevator_index].current_floor+1;}  //we model this for now as if we are sending elevators in their direction of travel one floor at a time.  This will not mean they actually stop at each floor though.  It's for how we are handling any change of events that happen as the elevator travels, which may require it to stop and open its doors. 
                if (elevator_direction == DOWN)
                    {elevators[elevator_index].current_floor = elevators[elevator_index].current_floor-1;}
                if ( elevators[elevator_index].current_floor >= NUM_FLOORS || elevators[elevator_index].current_floor < LOBBY )
                    {printf("Floor error: %d \t", elevators[elevator_index].current_floor);
                    //exit(0);
                    }
                if (elevator_direction == NO_DIRECTION)
                    {printf("No direction error \n");
                    exit(0);}
                elevators[elevator_index].elevator_time[elevator_direction] = time_per_floor + elevators[elevator_index].elevator_time[elevator_direction];
                Load_Event(&event_head , tnow + time_per_floor , ELEVATOR_ARRIVAL , ELEVATOR, elevator_index);
            } 
 
//sms: I've reviewed up to here so far. 
/**************************************************************************************************************************
This part takes care of the events when an elevator arrives at a floor. 
Note: this is a major piece of model logic. 

//HP: i haven't looked super carefully at this yet, but can you check that someone can get on the elevator who arrives while others are first unloading? 
//also: check that when elevator fills up, that the first person in any remaining queue "presses" the request button again.
**************************************************************************************************************************/
            if (next_event.event_type == ELEVATOR_ARRIVAL)  //elevator arrives at a floor.
            {   
                // get the elevator information
                elevator_index = next_event.entity_index;
                elevator_direction = elevators[elevator_index].direction;
                time_index = min(floor(tnow/60), BUILDING_HOURS-1 );  // in case the time exceeds the operating hours, we still assign the last working hour's idlingf floor.
                idle_floor = elevators[elevator_index].floor_idle[time_index]; //sms2: this and next line should only be needed to run if we find the elevator has nothing to do at the end of the code of this section. //hp: In this section, we just say what is the idling floor in this time, that we may need in the following chunk. 
                elevators[elevator_index].idle_floor = idle_floor;
                door_open = NO;  //hp: When an elevator arrives at a floor, we assume doors are closed. If doors remain closed, then we change its floor at the end of this section. If,however, elevator opens its door to offload people or to load new people, then "door_open=YES" and we will define a "Send_Elevator" at the end of this sestion to occur at time "tnow+2*DOOR_TIME".
                // For animation
                onload=0, offload=0; num_people_for_animation = elevators[elevator_index].num_people, direction_for_animation = elevator_direction;
                // Capture all elevators utilization
                stats[repnum].average_elevator_utilization_by_hour[time_index] = 0;
                for ( my_index = 0 ; my_index < NUM_ELEVATORS ; my_index++ )
                {
                    stats[repnum].average_elevator_utilization_by_hour[time_index] +=  elevators[my_index].elevator_time[UP] + elevators[my_index].elevator_time[DOWN];       
                }               



                // First, offload people that are already in the elevator.
                if (elevators[elevator_index].num_people > 0)
                {   
                    next_in_elevator = elevator_head[elevator_index]->Person;  //hossein4: to floor should not be less than current floor (for direction up)
                    while ( (elevators[elevator_index].num_people > 0) && (next_in_elevator.to_floor == elevators[elevator_index].current_floor) )
                    {
                        offload_total++; // For debugging purposes.
                        people[next_in_elevator.person_type][next_in_elevator.index].elevator_out_time = tnow + DOOR_TIME; // Door should get open for person to get off.
                        people[next_in_elevator.person_type][next_in_elevator.index].elevator_travel_time += people[next_in_elevator.person_type][next_in_elevator.index].elevator_out_time - people[next_in_elevator.person_type][next_in_elevator.index].elevator_start_travel_time ; //HP: why do we add this last part?  why do we bother keeping track of elevator travel time at the person level?  let's just time stamp for each individual when they get on the elevator; then when they get off, we just record their elevator travel time as tnow-this.person's.elevator.boarding.time (sms4: please note that each person uses the elevator twice, so we need adding second travel time to the first one.)
                        // Sanity check
                        if (people[next_in_elevator.person_type][next_in_elevator.index].elevator_travel_time < 0)
                            {printf("Elevator travel time error \n");
                            exit(0);}
                        // In the following "if" statement, we either load people into clinic queue, or send them directly into the clinic.
                        if (elevators[elevator_index].current_floor != LOBBY)
                        {   
                            offload_non_lobby++; // For debugging purposes.
                            people[next_in_elevator.person_type][next_in_elevator.index].current_floor = people[next_in_elevator.person_type][next_in_elevator.index].to_floor;
                            //hp: We can change the following line in order to enable travel between floors. E.g., someone can go to an upper floor to a labratory. 
                             //hp: For now I am working with Lobby, but we can generalize this. The current code can handle travel to upper floors from non-lobby floors.
                            people[next_in_elevator.person_type][next_in_elevator.index].to_floor = LOBBY;
                            Load_Event( &event_head ,  people[next_in_elevator.person_type][next_in_elevator.index].elevator_out_time   + CLINIC_TO_ELEVATOR_TIME, CLINIC_ARRIVAL , next_in_elevator.person_type, next_in_elevator.index);
                        }

       

                        Remove_Person_Elevator(&elevator_head[elevator_index]);
                        elevators[elevator_index].num_people --;
                        offload++;

                        if ( elevators[elevator_index].num_people > 0 ) // We will continue this "while" loop untill we are done with all the people who wants to drop off at this floor.
                            { next_in_elevator = elevator_head[elevator_index]->Person; }
                        door_open = YES; // We will need to know whether or not elevator has opened its doors at that floor.
                    } //end "while ( (elevators[elevator_index].num_people > 0) && (next_in_elevator.to_floor == elevators[elevator_index].current_floor) )"
                }//end  "if (elevators[elevator_index].num_people > 0)"
                /////////////////// End of offloading people from elevator
                /////////////////////////////////////////////////////////////////////////////////////////////////////
                    
                // The following 2 lines prevent unnecessary elevator trips to floors that has already been taken care of and there are no more people there.
                // If elevator is empty, then we double check that there are still people waiting at the elevator's final destination; If not we update its final destination.  
                // Note that elevator's idle floor is an exception; Even if there are no people waiting there, we still send the elevator to park at its idling floor.
                if ( elevators[elevator_index].num_people == 0 && queue_size[elevator_direction][elevators[elevator_index].final_destination] + queue_size[1-elevator_direction][elevators[elevator_index].final_destination] == 0 && elevators[elevator_index].final_destination != idle_floor )
                    {elevators[elevator_index].final_destination = elevators[elevator_index].current_floor;} //hossein4: If the sum ==0 part should be there. I may need a new field which specifies direction of the final destination.

                // If elevator has arrived at its final destination, it is empty, and there are no more people to be loaded at that floor, then we may implement a FIFO policy to send elevator to a floor where someone is waiting.
                if ( elevators[elevator_index].num_people == 0 && elevators[elevator_index].current_floor == elevators[elevator_index].final_destination && queue_size[elevator_direction][elevators[elevator_index].current_floor] == 0 ) 
                {   
                    // First, find where is the earlierst elevator request.               
                    earliest_request = 9999;
                    to_floor = BOGUS_VALUE;
                    for (storey = 0; storey < NUM_FLOORS; storey++) 
                        {   // Let us check if another elevator is assigned to that floor.
                            //hp: I am not sure if the elevator algorithm checks this or not; we can change this part easily if we do not want to check to see if another elevator is already assigned to that floor.
                            //hp: Note that implementing this increases the average wait time of people in the elevator queue. The reason is that on average there are 13 people in queues who want to go to lobby, so it is ok (and better) to send more than 1 elevator to a floor with non-zero queue size (but at the moment I am not doing that).
                            
                            counter = 0; // to count number of elevators assigned to that floor.//hossein4: if we generalize the code, then we need to make sure that in which direction an elevator is assigned to.
                            for (j=0; j<NUM_ELEVATORS; j++)
                            {
                                if (elevators[j].final_destination == storey)
                                    { counter++; }
                            }
                            // if another elevator is not assigned to that floor (i.e., counter==0), let us assign this elevator to that certain floor.
                            if ( counter == 0 || storey == elevators[elevator_index].current_floor)  // Note that the elevator number "elevator_index" is at its final destination. So there is one elevator whose final destination is "elevators[elevator_index].current_floor". But we can not say an elevator is already assigned to serve this floor, because the elevator # elevator_index is going in opposite direction compared to people locared at that floor. So we still need to consider people located at "elevators[elevator_index].current_floor, and assign the current elevator to them if they are the earliest people who has arrived.
                            {   // The following chunk finds the earliest elevator request at floors.
                                // An example will make this || above clear. An elevator which was going up has arrived at its final destination (floor 6). Elevator is empty and there are people who are waiting at that floor to go down. If they are the realiest request, we will chnage elevator's direction in the section below to serve them.
                                // If we do not use || above, the we need to uncomment "hoss1" below.
                                for (direction=0; direction < NUM_DIRECTIONS; direction++ )
                                {
                                    if (queue_size[direction][storey] > 0)
                                    {   
                                        next_in_Line = hall_head[direction][storey]->Person;
                                        if (people[next_in_Line.person_type][next_in_Line.index].line_up_time < earliest_request)
                                        {
                                            earliest_request = people[next_in_Line.person_type][next_in_Line.index].line_up_time;
                                            to_floor = storey;  
                                        }
                                    }
                                }                                               
                            }
                        }

                        if (to_floor >= 0) // if a floor is selected to send the elevator to.
                        {   
                            if (to_floor == elevators[elevator_index].current_floor) // If the earliest request is at the floor where elevator is currently located at, then we change its direction, because please recall that we have already checked above that there are no people at elevator's current floor who are going in the elevator's direction.
                                {elevator_direction = 1-elevator_direction;}
                            else
                            {elevator_direction = (to_floor <= elevators[elevator_index].current_floor );}

                            elevators[elevator_index].direction = elevator_direction;
                            elevators[elevator_index].final_destination = to_floor;
                            elevators[elevator_index].idle = NO;    
                        }
                        else 
                        {//if there are no floors where people are waiting for elevators with no elevator assigned to them, then send the elevator to its idling floor.  
                            if (elevators[elevator_index].current_floor > idle_floor)
                                {elevator_direction = DOWN;}
                            if (elevators[elevator_index].current_floor < idle_floor)
                                {elevator_direction = UP;}  // hossein: If you have people going up from non-lobby floor, you should write this one as an if statement and should take care of "="" case differently.
                            if (elevators[elevator_index].current_floor == idle_floor) // If the elevator is already at its idling floor, then make it idle.
                                {   //hoss1: if you do not use || in "hoss1" above, then make these 3 lines uncommented.
                                    //if (queue_size[1-elevator_direction][elevators[elevator_index].current_floor]>0) // If the elevator is at its idling floor, 
                                    //    {elevator_direction=1-elevator_direction;}
                                    //else 
                                        {elevator_direction = NO_DIRECTION;
                                        elevators[elevator_index].idle = 1;
                                        }
                                }
                            elevators[elevator_index].direction = elevator_direction;
                            elevators[elevator_index].final_destination = idle_floor; // Send elevator to its idle position.
                        }
                }

                // Load people in the elevator's current floor who are goin in the elevator's direction.
                if (elevator_direction != NO_DIRECTION)
                {   
                    while ( queue_size[elevator_direction][elevators[elevator_index].current_floor] > 0 && elevators[elevator_index].num_people < ELEVATOR_CAPACITY) 
                    {
                        next_in_Line = hall_head[elevator_direction][elevators[elevator_index].current_floor]->Person; // take the person from the wait list
                        people[next_in_Line.person_type][next_in_Line.index].elevator_wait_time[elevator_direction] = tnow - people[next_in_Line.person_type][next_in_Line.index].line_up_time + DOOR_TIME + people[next_in_Line.person_type][next_in_Line.index].elevator_wait_time[elevator_direction] ; //sms2: i didn't follow why the part from + DOOR_TIME and to the right //hp: we assume that the person waits until they get inside the elevator, so counting the "DOOR_OPEN" is required. The part after "DOOR_OPEN" is for the accounting purposes of previous wait times (wait time is being calculated cumulatively).
                        Load_Person_Elevator(&elevator_head[elevator_index], tnow + DOOR_TIME, next_in_Line.person_type, next_in_Line.index, people[next_in_Line.person_type][next_in_Line.index].to_floor, people[next_in_Line.person_type][next_in_Line.index].direction); // add people to elevator's list
                        people[next_in_Line.person_type][next_in_Line.index].elevator_start_travel_time = tnow + DOOR_TIME;


                        people[next_in_Line.person_type][next_in_Line.index].elevator_ind[repnum] = elevator_index;




                        
                        elevators[elevator_index].num_people ++; // update people's count in the elevators
                        onload++;
                        Remove_Person_Queue(&hall_head[elevator_direction][elevators[elevator_index].current_floor]);
                        //stats[repnum].average_hall_queue_size[elevator_direction] += (tnow - queue_registeration_time[elevators[elevator_index].current_floor])*(queue_size[UP][elevators[elevator_index].current_floor] + queue_size[DOWN][elevators[elevator_index].current_floor]);
                    
                        queue_size[elevator_direction][elevators[elevator_index].current_floor] -- ; // decrease number of people in the queue of each floor.
                        //queue_registeration_time[elevators[elevator_index].current_floor] = tnow;
                        door_open = YES; //sms2: where used? hp: If "door_open=YES", then we wait "2* DOOR_TIMME" for changing the elevator's floor. Please see the very end of this sesction for how I distinguish between  "door_open=YES" and "door_open=NO".
                        if (elevators[elevator_index].direction == UP)
                            elevators[elevator_index].final_destination = max( elevators[elevator_index].final_destination , people[next_in_Line.person_type][next_in_Line.index].to_floor);
                        if (elevators[elevator_index].direction == DOWN)
                            elevators[elevator_index].final_destination = min(elevators[elevator_index].final_destination , people[next_in_Line.person_type][next_in_Line.index].to_floor);                       
                    }
                }                     
                
                ///// Elevator has arrived at a floor and it is full.   
                //// if there are people waiting in the queue in this floor, but elevator had NO capacity to take those people, we need to re-request an elevator.  
                /// This should be after elevator changes its elevator_direction.
                // hhossein:This chunk is very immportant in assigning elevators to floors.
                if ( elevators[elevator_index].num_people == ELEVATOR_CAPACITY && queue_size[elevator_direction][elevators[elevator_index].current_floor] > 0) 
                {   
                    door_open = YES;
                    elevators[elevator_index].elevator_time[elevator_direction] = 2*DOOR_TIME + elevators[elevator_index].elevator_time[elevator_direction];
                    next_in_Line = hall_head[elevator_direction][elevators[elevator_index].current_floor]->Person;
                    // Only the person in the queue head needs to re-request the elevator for the whole queue (unless we have several elevator request pannels at each floor).        
                    Load_Event( &event_head, tnow +  2*DOOR_TIME+EPSILON, ELEVATOR_REQUEST, next_in_Line.person_type, next_in_Line.index ); // Note that "+EPSILON" part makes sure that elevator will leave the floor and we do not assign the same elevator.      
                }

                // Start of capturing Animation parameters

                if (onload > 0 || offload >  0)
                {   
                    if (direction_for_animation == UP)
                    {fprintf(animation_file, "%.2f \t elevator_load \t%d\t%d\t UP \t NA \t %d\t%d\t%d \n", tnow, elevators[elevator_index].current_floor, elevator_index, num_people_for_animation, offload, onload );
                    }
                    if (direction_for_animation == DOWN)
                    {fprintf(animation_file, "%.2f \t elevator_load \t%d\t%d\t DOWN \t NA \t %d\t%d\t%d \n", tnow, elevators[elevator_index].current_floor, elevator_index, num_people_for_animation, offload, onload );                        
                    } 
                }

                if (onload > 0) // It is important to capture this after elevator's direction changes. Consider for example an elevator that goes down to lobby, changes its direction and onloads new people.
                {   
                    if (elevators[elevator_index].direction == UP)
                    {fprintf(animation_file, "%.2f \t hall_queue \t%d\t NA\t UP \t NA \t %d\tNA\t NA \n", tnow + DOOR_TIME, elevators[elevator_index].current_floor, queue_size[elevator_direction][elevators[elevator_index].current_floor] );
                    }
                    if (elevators[elevator_index].direction == DOWN)
                    {fprintf(animation_file, "%.2f \t hall_queue \t%d\t NA\t DOWN \t NA \t %d\tNA\t NA \n", tnow + DOOR_TIME, elevators[elevator_index].current_floor, queue_size[elevator_direction][elevators[elevator_index].current_floor] );
                    } 
                }

                // End of capturing animation parameters

                if (elevators[elevator_index].idle == NO)
                {   // If door has never opened, then we can send the elevator right away and define an "elevator_arrival" in the next floor.
                    // But if the elevator has opened its door, we need to delay sending the elevator until its door gets closed.
                    
                    if (door_open == NO)
                    {   
                        elevators[elevator_index].current_floor = elevators[elevator_index].current_floor + 1 - 2 * (elevators[elevator_index].direction == DOWN); // If direction is down, this will decrement the current floor, and if direction is up, this will increment the current floor.                   
                        elevators[elevator_index].elevator_time[elevator_direction] = elevators[elevator_index].elevator_time[elevator_direction] + time_per_floor;
                        Load_Event( &event_head,tnow + time_per_floor, ELEVATOR_ARRIVAL , ELEVATOR, elevator_index);
                    }        

                    if (door_open == YES)
                    {   
                        elevators[elevator_index].elevator_time[elevator_direction] = 2 * DOOR_TIME + elevators[elevator_index].elevator_time[elevator_direction];
                        Load_Event( &event_head,tnow + 2*DOOR_TIME , SEND_ELEVATOR , ELEVATOR, elevator_index);
                        door_open = NO;                                                  
                    }
                }

            } //Ends part 3.



/**************************************************************************************************************************
Clinic arival event
**************************************************************************************************************************/
            if (next_event.event_type == CLINIC_ARRIVAL)  //Someone gets off teh elevator and arrives at the clinic
            {     
                clinic_num = people[next_event.entity_type][next_event.entity_index].clinic;
                //HP: we prob don't even care about recording get in clinic time for doctors and staff, do we? sms4: Please note that the CLINIC_COMPLETION event occures at get_in_clinic_time + appointment_duration. So we define the get_in_clinic_time for everyone to make the definitions consistent for evenyone in the system.
                if (next_event.entity_type == DOCTOR || next_event.entity_type==STAFF ) // Doctors and staff get directly into the clinic.
                {   
                    people[next_event.entity_type][next_event.entity_index].get_in_clinic_time =tnow;                                
                    // load "PERSON_ARRIVES_ELEVATOR_HALL" event for each person. This is the event when the person is out of clinic and wants to go back to the lobby and leave the system.              
                    //HP: do doctors and staff have appoitnment durations?  don't we just have start and end time of their shift?  next line is using appointment duration
                    //sms4: At the above line, doctors and staff do have appointment durations, which is defined as their clinic_end_time - clinic_start_time. 
                    Load_Event( &event_head , tnow + people[next_event.entity_type][next_event.entity_index].appointment_duration , CLINIC_COMPLETION , next_event.entity_type , next_event.entity_index );                                  
                }
                if ( next_event.entity_type == PATIENT ) // If the person type is "Patient", they may go to clinic immediately, or they may line up.
                {                                                                                       
                    if ( clinics[clinic_num].num_patients < clinics[clinic_num].num_docs ) //If there are available doctors.
                    {   //person gets into the clinic immediately
                        clinics[clinic_num].num_patients++; 
                        people[next_event.entity_type][next_event.entity_index].get_in_clinic_time = tnow;
                        // hossein: note that if we consider travel to other floors as well, say for example to a lab after the clinic, then we need to re-define parameters like appointment duration. The following line is one of the places where this matters. You can re-define these parameters in the "CLINIC_COMPLETION" only if they would not travel to lobby after their appointment.                                                      
                        Load_Event( &event_head , tnow + people[next_event.entity_type][next_event.entity_index].appointment_duration , CLINIC_COMPLETION , next_event.entity_type, next_event.entity_index);                                    
                    }
                    else
                    {   // Add person to the clinic queue.
                        Load_Person_Queue( &clinic_head[clinic_num] , tnow, next_event.entity_type, next_event.entity_index);
                        clinic_queue_length[clinic_num]++; // "clinic_queue_length" is the length of the queue in front of each clinic.
                        stats[repnum].max_clinic_queue_length = max(clinic_queue_length[clinic_num], stats[repnum].max_clinic_queue_length); 

                        people[next_event.entity_type][next_event.entity_index].clinic_line_up_time = tnow;

                        // Start Capturing animation parameters
                        // NOTE: I am only capturing this when their clinic is full
                        fprintf(animation_file, "%.2f \t doctor_queue \t%d\t NA\t NA \t %d \t %d\tNA\t NA \n", tnow, people[next_event.entity_type][next_event.entity_index].current_floor, clinic_num, clinic_queue_length[clinic_num]);
                        // End Capturing animation parameters
                    
                    
                    }
                }                                                   
            }
/**************************************************************************************************************************
Clinic departure event
**************************************************************************************************************************/
            if (next_event.event_type == CLINIC_COMPLETION)  
            {   //HP: It would feel cleaner/more modular if you let CLINIC_COMPLETION be an event that we load as the start time + duration.  Then upon clinic completion, we do the updating of 
                //whether someone should move into clinic service, update stats on the clinic, and then load the event PATIENT_ARRIVES_ELEVATOR_HALL, which can be epsilon time later, or we can load it to be some travel time.  it just allows more organized approach.
                //I think the logic here in combo with the logic further below about a person first arriving and immediately joining the clinic and loading the elevator hall arrival event works logically though.
                
                //In this chunk, we replace the person who gets out of the clinic with another person from the clinic queue who was waiting to get into the clinic.
                ////////////////////////////////////////////////////
                /////////////// START            
                if (next_event.entity_type == PATIENT ) // For now, we only consider the type "patient" as using elevators to go from upper to lower floors
                    //HP: important to note this point that we are not sending doctors/staff down, only up in this version of the model!  we should update that soon. (sms4: could we please discuss the logoc behind this?)
                {
                    clinic_num = people[next_event.entity_type][next_event.entity_index].clinic;
                    clinics[clinic_num].num_patients--;
                    if ( clinic_queue_length[clinic_num] > 0 )
                    {
                        next_in_clinic_queue = clinic_head[clinic_num]->Person;
                        people[next_in_clinic_queue.person_type][next_in_clinic_queue.index].get_in_clinic_time = tnow;
                        clinic_queue_length[clinic_num]--;
                        Remove_Person_Queue(&clinic_head[clinic_num]);
                        people[next_in_clinic_queue.person_type][next_in_clinic_queue.index].appointment_wait_time = tnow  - people[next_in_clinic_queue.person_type][next_in_clinic_queue.index].clinic_line_up_time;
                        clinics[clinic_num].num_patients++; //HP: do we ever use this info? sms4: yes, actually this is an important stat which helps us decide whether we can send a person to the clinic right after they are out of elevator, or we need adding them to the waiting list of the clinic.
                        Load_Event(&event_head , tnow + people[next_in_clinic_queue.person_type][next_in_clinic_queue.index].appointment_duration , CLINIC_COMPLETION , next_in_clinic_queue.person_type , next_in_clinic_queue.index );                                
                    
                        // We define a "PERSON_ARRIVES_ELEVATOR_HALL" for this patinet for the time that they are done with their appointment.                               
                        // Start Capturing animation parameters
                        // NOTE: I am only capturing this when clinic completion leads to change in the clinic_queue_length
                        fprintf(animation_file, "%.2f \t doctor_queue \t%d\t NA\t NA \t %d \t %d\tNA\t NA \n", tnow, people[next_event.entity_type][next_event.entity_index].current_floor, clinic_num, clinic_queue_length[clinic_num]);
                        // End Capturing animation parameters
                    
                    }

                }
                                                ////////////////END
                ///////////////////////////////////////////////////
                Load_Event( &event_head , tnow + CLINIC_TO_ELEVATOR_TIME , PERSON_ARRIVES_ELEVATOR_HALL , next_event.entity_type, next_event.entity_index);
            }

/**************************************************************************************************************************
  This part takes care of the events when there is an elevator request (by a person) at a floor.
//sms2: easier if you talk me through this. 
**************************************************************************************************************************/

            if (next_event.event_type == ELEVATOR_REQUEST)
            {
                elevator_call++; // counts the number of elevator calls

                person_direction = people[next_event.entity_type][next_event.entity_index].direction;
                person_to_floor = people[next_event.entity_type][next_event.entity_index].to_floor;
                person_current_floor = people[next_event.entity_type][next_event.entity_index].current_floor;

                if ( queue_size[person_direction][person_current_floor] > 0 ) // We only request an elevator if there are still people waiting at that floor (they may have been taken care of sooner by another elevator).  //sms2: this shouldn't be the case given that this request event happens at tnow + eps // hp; please see the line below.
                {//hp: There are cases where the "if" statement above may not be satisfied. Consider an elevator arrival at floor 9 exactly at the same time with a clinic departure at floor 9. Let us say first we read the clinic departure from the event list, and there are no elevators available at floor 9.
                //hp: so we place an "elevator_request" for tnow+EPSILON. However, before the "elevator_request" event, the elevator arrival gets read from the event list, elevator changes direction and loads this one person. It is rare but sometimes it happens.  
                
                // Let us find the elevator that will get to this person the soonest.
                //hp: please note that I am considering all the elevators, except those elevators that are in the same direction with this person, but have passed this person.
                //hp: please note that we can easily play with this part of the code and do not consider some elevators by letting "elevators[i].time_to_reach=999".

                //hossein: I have condensed this for-loop, and I have removed updating the elevators final destination if it has people in it (I think it is unnecessary).
                //hossein: But if later you see an error and you want to see the original un-condensed version, see the buttom of "diomond_clinic6.c". Search for "uncondensed1".
                //////////////////////////////////////////////////////////////////////////////////////
                //////////////////////START
                wait_time = 999; // initialize wait_time to something large
                for (i = 0; i < NUM_ELEVATORS; i++)
                {                                              
                    if ( elevators[i].idle == NO )
                    { 
                        if ( elevators[i].direction == person_direction ) // If elevator is in the same direction with the person.
                        {                              
                            if ( elevators[i].current_floor >= person_current_floor ) // We only consider the elevators that have not yet reached the person.
                            {distance = elevators[i].current_floor - person_current_floor + 999999 * (person_direction == UP);}// If person's direction is up, then this will be  alarge number and this elevator is out of candidates elevator list.                                   
                            else
                            { distance = person_current_floor - elevators[i].current_floor + 999999 * (person_direction == DOWN);} // For example, if elevator is going down and is in lower floor compared to the person (who is also going down), we do not consider this elevator until it gets idle.                                                                                         
                        }
                        if (elevators[i].direction == 1-person_direction) // If elevator is in opposite direction compared to person, we still consider that. But we can remove these levators by letting their distance be 999999.
                        {   
                            if (elevators[i].direction == UP)
                            {   
                                if ( elevators[i].final_destination <= person_current_floor) // if the elevator is going to an upper floor compared to this person, then it can take care of this person on its way down, so do not need to do anything. 
                                // however, if elevator e.g. is going to floor 8 and a person is waiting at floor 9, we may send this elevator to take care of this person as well.
                                {distance = person_current_floor - elevators[i].current_floor;}
                                else
                                {distance = 999999;}
                            }
                            if (elevators[i].direction == DOWN)
                            {distance = elevators[i].current_floor;} // We have this because people may go up only from Lobby. But we should change this if we consider travel to upper floors from a non-lobby floor.    
                        }                                                                                                                 
                    }   

                    if (elevators[i].idle == YES)
                        {distance = abs(( elevators[i].current_floor - person_current_floor ));}                                                 
                    // If elevator has loaded people and we still need more elevators, we use the following trick so that the same elevator will not be assigned.
                    if ( elevators[i].num_people == ELEVATOR_CAPACITY && elevators[i].current_floor == person_current_floor)
                        {distance = 999999;}
                    if ( distance < 0 ) // sanity check
                        {printf("Distance claculation error1 \n");
                        exit(0);}
                    //hp: In the following line, we may refine this estimation later by considering door_times.
                    elevators[i].time_to_reach = time_per_floor * distance;
                    if (elevators[i].time_to_reach < wait_time)
                        { wait_time = elevators[i].time_to_reach;}
                }
                                                                                //////////////////////END
                //////////////////////////////////////////////////////////////////////////////////////
                // Now that we have found the minimum wait time of a person until elevator reaches them, let us assign one of the elevators that will arrive to this person the soonest.
                num_ele=0; // Number of elevators that will arrive to this person the soonest.
                for (k=0; k<NUM_ELEVATORS; k++)
                {   
                    if (wait_time == elevators[k].time_to_reach) 
                        {num_ele++;}
                }
                
                //printf("%d \t",num_ele);
                //printf("%f \t",wait_time);
                // find the elevator k that reaches to this person the soonest.
                if ( num_ele > 0 ) 
                    {   
                        // Randomly assign one of the elevators that will arrive to this person the soonest (we may have for example two candidate elevators).
                        prob = 1.0 / num_ele;
                        unif = Unif();              
                        num_ele = 0;
                        ele_index = BOGUS_VALUE;
                        k = 0;
                        while ( ele_index<0 && k<NUM_ELEVATORS)
                        {
                        if ( wait_time == elevators[k].time_to_reach )
                            { num_ele++; }
                        if (unif < num_ele * prob)
                            { ele_index=k; }                                                                            
                        k++;
                        }

                        if  ( ele_index >= 0 ) //sms2: since we eare in num_evel > 0, doesn't this condition need to be true? (sms4: I am not sure if I got this)// If you have been able to select an elevator, then assign that elevator to person's floor.
                        {   
                            if ( wait_time >= 999 ) //hossein4: equality of floats
                            {
                                printf("Error: wait time is %f \t", wait_time);
                                exit(0);
                            }

                            // We need assigning the elevator number "ele_index" to the person's floor.

                            if (elevators[ele_index].direction == UP)
                                { elevators[ele_index].final_destination = max(person_current_floor , elevators[ele_index].final_destination );}
                            if (elevators[ele_index].direction == DOWN)
                                { elevators[ele_index].final_destination = min(person_current_floor , elevators[ele_index].final_destination);}
                             
                            // If elevator is idle, we need moving that elevator.                       
                            if (elevators[ele_index].idle == YES)
                            {   
                                //  printf("Selected elevator is:%d \n", ele_index);
                                if (elevators[ele_index].current_floor == person_current_floor)
                                {
                                    printf("Error4 \n");
                                    exit(0); // This should not happen, because if an elevator was available, then it should have loaded the person in the "PERSON_ARRIVES_ELEVATOR_HALL" section.
                                }
                                else
                                {
                                    direction_indicator = (elevators[ele_index].current_floor >= person_current_floor);
                                    elevators[ele_index].direction = direction_indicator;
                                }
                                                                   
                                elevators[ele_index].final_destination = person_current_floor;
                                Load_Event( &event_head , tnow , SEND_ELEVATOR , ELEVATOR , ele_index ); 
                                elevators[ele_index].idle = NO;    
                            } 
                        }                                        
                    }                   
                }   
            }
/**************************************************************************************************************************
 Summary statistics go down here.
*************************************************************************************************************************/

            if (num_events_on_calendar == 0) //this indicates this simulation replication is over
                keepgoing = 0;

        } //end while keepgoing
         // records all stats for each replication 

        
        //record KPIs from this replication
      //end for each rep
    //print stats results
    
    // Calculte the total wait tiem for all peopel
    
    //double total_elevator_wait = 0.0;
    //double average_elevator_wait = 0.0;
    //double total_appointment_wait = 0.0;
    //double average_appointment_wait = 0.0;

    // Capture people wait time for this replication
    double doc_staff_array[(num_docs+num_staff)]; 
    double patient_array[num_patients] ; 
    for ( my_type = 0 ; my_type < 3 ; my_type++)
    {
        if (my_type == PATIENT)
        {
            for (my_index = 0; my_index < num_patients; my_index++ )
            {
                if (people[my_type][my_index].elevator_wait_time[UP] < 0 || people[my_type][my_index].elevator_wait_time[DOWN] < 0 )
                    {printf("Wait time error in replication %d \n", repnum);
                    exit(0);}
                stats[repnum].count_patient_to_floors[people[my_type][my_index].current_floor]++;

                patient_array[my_index] = people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN];
                stats[repnum].average_elevator_wait_patient += people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN];
                stats[repnum].max_elevator_wait_patient = max( people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN], stats[repnum].max_elevator_wait_patient );
                
                stats[repnum].average_elevator_wait_all_people += people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN];
                stats[repnum].max_elevator_wait_all_people = max( people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN], stats[repnum].max_elevator_wait_all_people );
                
                
                
                // Max wait time by floors
                stats[repnum].max_elevator_wait_by_floor_patient[people[my_type][my_index].current_floor ] =  max( people[my_type][my_index].elevator_wait_time[DOWN] , stats[repnum].max_elevator_wait_by_floor_patient[people[my_type][my_index].current_floor ] );
                stats[repnum].max_elevator_wait_by_floor_patient[LOBBY] =  max( people[my_type][my_index].elevator_wait_time[UP] , stats[repnum].max_elevator_wait_by_floor_patient[LOBBY] );

                //count_patient_to_floors[people[my_type][my_index].current_floor] ++;
            
                stats[repnum].average_elevator_travel_time += people[my_type][my_index].elevator_travel_time;
                stats[repnum].max_elevator_travel_time = max( people[my_type][my_index].elevator_travel_time, stats[repnum].max_elevator_travel_time );

                stats[repnum].average_appointment_wait += people[my_type][my_index].appointment_wait_time;
                stats[repnum].max_appointment_wait = max( people[my_type][my_index].appointment_wait_time , stats[repnum].max_appointment_wait);
            
            }
        }
        if (my_type == DOCTOR )
        {
            for (my_index = 0; my_index < num_docs; my_index++ )
            {   
                stats[repnum].count_doc_staff_to_floors[people[my_type][my_index].current_floor]++;

                doc_staff_array[my_index] = people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN];
                stats[repnum].average_elevator_wait_doc_staff += people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN];
                stats[repnum].max_elevator_wait_doc_staff = max( people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN], stats[repnum].max_elevator_wait_doc_staff );  
                
                stats[repnum].average_elevator_wait_all_people += people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN];
                stats[repnum].max_elevator_wait_all_people = max( people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN], stats[repnum].max_elevator_wait_all_people );                

                stats[repnum].average_elevator_travel_time += people[my_type][my_index].elevator_travel_time;
                stats[repnum].max_elevator_travel_time = max( people[my_type][my_index].elevator_travel_time, stats[repnum].max_elevator_travel_time );
            }
        }
        if ( my_type == STAFF)
        {
            for (my_index = 0; my_index < num_staff; my_index++ )
            {
                stats[repnum].count_doc_staff_to_floors[people[my_type][my_index].current_floor]++;

                doc_staff_array[num_docs+my_index] = people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN];
                stats[repnum].average_elevator_wait_doc_staff +=  people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN];
                stats[repnum].max_elevator_wait_doc_staff = max( people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN], stats[repnum].max_elevator_wait_doc_staff );  

                stats[repnum].average_elevator_wait_all_people += people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN];
                stats[repnum].max_elevator_wait_all_people = max( people[my_type][my_index].elevator_wait_time[UP] + people[my_type][my_index].elevator_wait_time[DOWN], stats[repnum].max_elevator_wait_all_people );                
                

                stats[repnum].average_elevator_travel_time += people[my_type][my_index].elevator_travel_time;
                stats[repnum].max_elevator_travel_time = max( people[my_type][my_index].elevator_travel_time, stats[repnum].max_elevator_travel_time );
            }
            
        }
    }
    
    stats[repnum].median_elevator_wait_patient = Find_median(patient_array, num_patients);
    stats[repnum].average_elevator_wait_patient /= num_patients;

    stats[repnum].median_elevator_wait_doc_staff = Find_median(doc_staff_array, (num_docs+num_staff) );
    stats[repnum].average_elevator_wait_doc_staff /= (num_docs+num_staff);

    stats[repnum].average_elevator_wait_all_people /= (num_patients+num_docs+num_staff);

    stats[repnum].average_elevator_travel_time /= lobby_arrival;
    stats[repnum].average_appointment_wait /= num_patients;

    stats[repnum].num_docs = num_docs;
    stats[repnum].num_staff = num_staff;
    stats[repnum].num_patients = num_patients;
    stats[repnum].total_num_people = num_patients+num_docs+num_staff;




    
    //for (storey = 0 ; storey < NUM_FLOORS ; storey++)
    //{ 
        //stats[repnum].average_elevator_wait_by_floor[storey] /= count_patient_to_floors[storey];
    //  stats[repnum].average_hall_queue_size[storey] /= tnow;
    //  stats[repnum].average_hall_queue_size_across_floors += stats[repnum].average_hall_queue_size[storey];
    //} 

    //stats[repnum].average_hall_queue_size_across_floors /= NUM_FLOORS ;          
        
    //printf("Average total wait time (up and down) in the elevator queue is: %.2f minutes \n", average_elevator_wait);
    //printf("Average appointment wait time is: %.2f minutes \n", average_appointment_wait);
    // Calculte the total time all elevators have travelled
    //double total_elevator_travel = 0.0;
    //double average_elevator_travel = 0.0;

    ////////////////////////////////////////////////////
    // Capture elevator information for this replication
    ///////////////////////////////////////////////////
    for ( my_index = 0 ; my_index < NUM_ELEVATORS ; my_index++ )
    {
            stats[repnum].average_elevator_in_use_time +=   elevators[my_index].elevator_time[0] + elevators[my_index].elevator_time[1];
            stats[repnum].average_elevator_utilization +=  elevators[my_index].elevator_time[0] + elevators[my_index].elevator_time[1];       
            stats[repnum].elevator_utilization[my_index] = (elevators[my_index].elevator_time[0] + elevators[my_index].elevator_time[1]) / tnow ;
    }
    
    stats[repnum].average_elevator_in_use_time /= NUM_ELEVATORS ;
    stats[repnum].average_elevator_utilization /=  ( tnow * NUM_ELEVATORS);

    for (i = 1; i <= min(floor(tnow/60), BUILDING_HOURS-1 );  i++)
    {   
        for (j = 0; j < i;  j++)
            stats[repnum].average_elevator_utilization_by_hour[i] -=  stats[repnum].average_elevator_utilization_by_hour[j];
    }       

    for (i = 0; i< BUILDING_HOURS;  i++)
        stats[repnum].average_elevator_utilization_by_hour[i] /= ( 60*NUM_ELEVATORS);

            
    
    
    //printf("Average elevator travel time is: %.2f minutes \n", average_elevator_travel);
    //hossein4; another important KPI is each person's elevator travel time.

    // Check to see if the system empty?
    for (direction=0 ; direction < NUM_DIRECTIONS ; direction++)   
        {   
            for (storey = 0 ; storey < NUM_FLOORS ; storey++)
            { 
                if ( queue_size[direction][storey] > 0 )
                    {printf("Hall queue error \n"); 
                    break;}             
            }
        }

    for (j= 0; j <  NUM_CLINICS; j++)
    {
        if ( clinic_queue_length[j] > 0 )
            {printf("Clinic queue error \n");
            break;}
    }



    
    Print_Calendar();
    fclose(output_file);
    

}//End of repllication "For"

print_results(stats, total_reps);
//char ff[30]= "average_elevator_motion_time";
//printf("%f \t",(stats->ff)  );

printf("done \n");


}  //end for main()



/********************************************************************************************
Open_and_Read_Files() opens all output and input files, and reads the latter into appropriate data structures 
********************************************************************************************/
void initialize_vars(statistics* stats)
{
	register int i, j;

    stats->average_elevator_travel_time = 0.0;
    stats->max_elevator_travel_time = 0.0;
    
    stats->average_elevator_wait_patient = 0.0;
    stats->average_elevator_wait_doc_staff = 0.0;
    stats->average_elevator_wait_all_people = 0.0;
    stats->average_appointment_wait = 0.0;

    
    stats->max_clinic_queue_length = 0.0;
    stats->max_elevator_wait_patient = 0.0;
    stats->max_elevator_wait_doc_staff = 0.0;
    stats->max_elevator_wait_all_people = 0.0;
    //stats->average_hall_queue_size_across_floors = 0.0;
    

    for (i = 0; i < NUM_FLOORS; i++)
	{   
        stats->max_hall_queue_size[i] = 0.0;
        stats->max_elevator_wait_by_floor_patient[i] = 0;

        stats->count_doc_staff_to_floors[i] = 0;
        stats->count_patient_to_floors[i] = 0;
        //count_patient_to_floors[i] = 0;
		//stats->average_hall_queue_size[i] = 0.0;
    }

    stats->average_elevator_in_use_time = 0.0;
    stats->average_elevator_utilization = 0.0;

    for (i=0; i < NUM_ELEVATORS; i++)
        stats->elevator_utilization[i] = 0.0;

    for (i=0; i<BUILDING_HOURS; i++)
        stats->average_elevator_utilization_by_hour[i] = 0;      
        
}

void print_results(statistics* stats, int total_reps)
{	
    double avg, stddev, maximum;
    double vector[REPS];
	int i,j;
    gen_results = fopen("gen_results.txt","w");
    if (gen_results == NULL)
	{
		printf("error: Failed to open gen_results.txt\n");
		return;
	}



    fprintf(gen_results, "******************************************************************* \n");
    fprintf(gen_results, "People wait parameters  \n");
    fprintf(gen_results, "******************************************************************* \n");
    fprintf(gen_results, "Variable \t \t \t \t \t \t \t  \t \t \t \t");
    fprintf(gen_results, "Mean  \t ");
    fprintf(gen_results, "stddev \t ");
    fprintf(gen_results, "maximum \t \t");
    fprintf(gen_results, "95%% CI ");
    fprintf(gen_results, "\n");

	fprintf(gen_results, "All people wait time in hall queue (UP & DOWN): \n");
//////////////////////////////////////////////////////////    
	fprintf(gen_results, "Average elevator wait time \t \t \t \t \t \t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].average_elevator_wait_all_people;
    help_print_results(vector);
//////////////////////////////////////////////////////////    
	fprintf(gen_results, "Maximum elevator wait time \t \t \t \t \t \t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].max_elevator_wait_all_people;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Docs and staff wait time in hall queue (UP & DOWN): \n");    
	fprintf(gen_results, "Average elevator wait time \t \t \t \t \t \t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].average_elevator_wait_doc_staff;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Max elevator wait\t \t \t \t \t \t \t \t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].max_elevator_wait_doc_staff;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Median elevator wait\t\t \t \t \t \t \t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].median_elevator_wait_doc_staff;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Patient's wait time in hall queue (UP & DOWN): \n");

	fprintf(gen_results, "Average elevator wait time \t \t \t \t \t \t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].average_elevator_wait_patient;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Max elevator wait\t \t \t \t \t \t \t \t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].max_elevator_wait_patient;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Median elevator wait\t\t \t \t \t \t \t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].median_elevator_wait_patient;
    help_print_results(vector);

	fprintf(gen_results, "Max elevator wait time for patients at each floor: \n");
    for (storey = 0; storey < NUM_FLOORS; storey++) 
    {
        fprintf(gen_results, "Floor %d \t \t \t \t \t \t \t \t \t \t  ", storey);
        for (j = 0; j < total_reps; j++)
        {           
            vector[j] = stats[j].max_elevator_wait_by_floor_patient[storey];  
        }
        help_print_results(vector);
    }

//////////////////////////////////////////////////////////
	fprintf(gen_results, " \nAverage elevator travel time (All people) \t\t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].average_elevator_travel_time;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Max elevator travel time (All people)\t\t\t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].max_elevator_travel_time;
    help_print_results(vector);

//////////////////////////////////////////////////////////
	fprintf(gen_results, "\nAverage appointment wait time (patients)  \t\t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].average_appointment_wait;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Max appointment wait time (patients)  \t\t\t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].max_appointment_wait;
    help_print_results(vector);

    fprintf(gen_results, "\n******************************************************************* \n");
    fprintf(gen_results, "Elevator parameters \n");
    fprintf(gen_results, "******************************************************************* \n");
    fprintf(gen_results, "Variable \t \t \t \t \t \t \t \t \t");
    fprintf(gen_results, "Mean  \t ");
    fprintf(gen_results, "stddev \t ");
    fprintf(gen_results, "maximum \t \t");
    fprintf(gen_results, "95%% CI ");
    fprintf(gen_results, "\n");



    //////////////////////////////////////////////////////////    
	fprintf(gen_results, "Average time elevator in use \t  \t \t\t");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].average_elevator_in_use_time;
    help_print_results(vector);
    //////////////////////////////////////////////////////////    
	fprintf(gen_results, "Average elevator utilization \t \t  \t  ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].average_elevator_utilization;
    help_print_results(vector);
    //////////////////////////////////////////////////////////    
	fprintf(gen_results, "\nElevator utilization of each elevator: \n");
    for (my_index=0; my_index < NUM_ELEVATORS; my_index++)
    {   
        fprintf(gen_results, "Utilization of E%d \t \t \t \t \t \t  ", 1+my_index);
        for (j = 0; j < total_reps; j++)
            vector[j] = stats[j].elevator_utilization[my_index];
        help_print_results(vector);
    }
    //////////////////////////////////////////////////////////    
	fprintf(gen_results, "\nAverage elevator utilization at each hour: \n");
    for (my_index=0; my_index < BUILDING_HOURS; my_index++)
    {   
        if (my_index+1 < 10)
            fprintf(gen_results, "Hour  %d \t \t \t \t \t \t \t      ", 1+my_index);
        else
            fprintf(gen_results, "Hour %d \t \t \t \t \t \t \t      ", 1+my_index);
        
        for (j = 0; j < total_reps; j++)
            vector[j] = stats[j].average_elevator_utilization_by_hour[my_index];
        help_print_results(vector);
    }

   // Queue information
    fprintf(gen_results, " \n******************************************************************* \n");
    fprintf(gen_results, "Queue information \n");
    fprintf(gen_results, "******************************************************************* \n");
    fprintf(gen_results, "Variable \t \t \t \t \t \t \t");
    fprintf(gen_results, "Mean  \t ");
    fprintf(gen_results, "stddev \t  ");
    fprintf(gen_results, "maximum \t \t");
    fprintf(gen_results, "95%% CI ");
    fprintf(gen_results, "\n");

	fprintf(gen_results, "Max hall queue size across floors  ");
    for (j = 0; j < total_reps; j++)
    {   
        vector[j] = 0;
        for (storey = 0; storey < NUM_FLOORS; storey++) 
            vector[j] = max(vector[j],  stats[j].max_hall_queue_size[storey]);    
    }
    help_print_results(vector); 
///////////////////////////////////////////////////////
    fprintf(gen_results, "Max hall queue size by floor:");
    fprintf(gen_results, "\n");
    for (storey = 0; storey < NUM_FLOORS; storey++) 
    {   
        fprintf(gen_results, "Floor %d \t \t \t \t \t \t   ", storey);
        for (j = 0; j < total_reps; j++)
            vector[j] = stats[j].max_hall_queue_size[storey];
        help_print_results(vector);
    }
//////////////////////////////////////////////////////////
    fprintf(gen_results, "\nMax clinic queue size \t \t \t   ");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].max_clinic_queue_length;
    help_print_results(vector);
//////////////////////////////////////////////////////////
   // People destination informmation
    fprintf(gen_results, " \n******************************************************************* \n");
    fprintf(gen_results, "People destination \n");
    fprintf(gen_results, "******************************************************************* \n");
    fprintf(gen_results, "Variable \t \t \t \t \t \t \t");
    fprintf(gen_results, "Mean  \t ");
    fprintf(gen_results, "stddev \t  ");
    fprintf(gen_results, "maximum \t \t");
    fprintf(gen_results, "95%% CI ");
    fprintf(gen_results, "\n");
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Total numbe of people:\t \t \t \t");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].total_num_people;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Number of patients:\t \t \t \t\t");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].num_patients;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Number of doctors:\t \t \t \t\t");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].num_docs;
    help_print_results(vector);
//////////////////////////////////////////////////////////
	fprintf(gen_results, "Number of staff:\t \t \t \t\t");
    for (j = 0; j < total_reps; j++)
        vector[j] = stats[j].num_staff;
    help_print_results(vector);
//////////////////////////////////////////////////////////

	fprintf(gen_results, "Number of patients going to floor X:\n");
    for (storey=1; storey < NUM_FLOORS; storey++)
    {   
        fprintf(gen_results, "Floor%d \t \t \t \t \t \t \t   ", storey);
        for (j = 0; j < total_reps; j++)
            vector[j] = stats[j].count_patient_to_floors[storey];
        help_print_results(vector);
    }

	fprintf(gen_results, "\nNum of docs&staff going to floor X:\n");
    for (storey=1; storey < NUM_FLOORS; storey++)
    {   
        fprintf(gen_results, "Floor%d \t \t \t \t \t \t \t   ", storey);
        for (j = 0; j < total_reps; j++)
            vector[j] = stats[j].count_doc_staff_to_floors[storey];
        help_print_results(vector);
    }

}


void help_print_results(double vector[REPS])
{	
    int reps = REPS;
    double stddev, avg, maximum ;
    stddev = avg = maximum = 0;
	for (j = 0; j < reps; j++)
		{avg += vector[j];
        maximum = max( maximum,  vector[j]);}
        
	avg /= reps;
    if (avg>=100)
	    fprintf(gen_results, "%.2f \t", avg);
    if (avg>=10 & avg <100)
        fprintf(gen_results, " %.2f \t", avg);
    if (avg<10)
        fprintf(gen_results, "  %.2f \t", avg);

	if (reps > 1)
	{   
		for (j = 0; j < reps; j++)
			stddev += pow(((double)(vector[j] ) - avg),2);               
		

		stddev = sqrt(stddev/(reps-1));
		fprintf(gen_results, " %.4f \t ", stddev);
        fprintf(gen_results, " %.4f \t ", maximum);
        Z95 = 1.959964;
        RCI = avg + Z95*(stddev / sqrt(reps));
        LCI = avg - Z95*(stddev / sqrt(reps));
        fprintf(gen_results, "  (%.2f,", LCI);
        fprintf(gen_results, "%.2f)", RCI);
        
        fprintf(gen_results, "\n");
    }
}

void Open_and_Read_Files()
{
    
    char buf[1024];  //holds line of data at a time

    int i, row, col, clinic_index;
    total_docs=0;

    output_file = fopen("/Users/hosseinpiri/Desktop/Diamond_clinic/Diamond_Elevator_Project/output.txt","w");
    
    animation_file = fopen("/Users/hosseinpiri/Desktop/Diamond_clinic/Diamond_Elevator_Project/animation.txt","w");
    unif = Unif();
    num_clinic_filles=3.0;
    //clinic_file = fopen("/Users/hosseinpiri/Desktop/Diamond_clinic/Diamond_Elevator_Project/Clinic_Input_File.csv","r");

    //if (unif<=1.0/num_clinic_filles)
    clinic_file = fopen("/Users/hosseinpiri/Desktop/Diamond_clinic/Diamond_Elevator_Project/Clinic_new_data_sample1.csv","r");
    //else if (unif<=2.0/num_clinic_filles)
      //  clinic_file = fopen("/Users/hosseinpiri/Desktop/Diamond_clinic/Diamond_Elevator_Project/Clinic_new_data_sample2.csv","r");
    //else if (unif<=3.0/num_clinic_filles)
        //clinic_file = fopen("/Users/hosseinpiri/Desktop/Diamond_clinic/Diamond_Elevator_Project/Clinic_new_data_sample3.csv","r");
    
        
    
    

    
    elevator_file = fopen("/Users/hosseinpiri/Desktop/Diamond_clinic/Diamond_Elevator_Project/Elevator_Input_File.csv","r");

    // print header of Raghav's animation file
    fprintf(animation_file, "time \t eventName \t floorNum\t eleNum \t eleDir \t clinicNum \t newVal \t to_Drop_now \t to_Load_now \n");
    
    //READ IN CLINIC INFO information sheet one line at a time
    row = 0;
    while (fgets(buf, 1024, clinic_file))
    {
        if (row == 0 || row == 1)   //get past first two header rows
        {
            row++;
            continue;
        }
        if (row >= NUM_CLINICS + 2) //the +2 for the two header rows
            break;

        clinic_index = row - 2;
        char* field = strtok(buf, ",");

        for (col = 0; col < 4; col++)  //read in first 5 columns (through and including num_docs)
        {
            if (col > 0)
                field = strtok(NULL, ",");
            switch (col)
            {
            case 0:
                clinics[clinic_index].index = atoi(field);
                break;
            case 1:
                clinics[clinic_index].floor = atoi(field);
                break;
            case 2:
                clinics[clinic_index].num_staff = atoi(field);
                break;
            case 3:
                clinics[clinic_index].num_docs = atoi(field);
                break;

            default:
                // should never get here
                //printf("Should never get to this default in switch\n");
                //exit(0);
                break;
            }
        }

        for (i = 0; i < clinics[clinic_index].num_docs; i++)
        {
            
            if (i==0)
            {
                field = strtok(NULL, ",");
                clinics[clinic_index].num_pats[i] = atoi(field);
                field = strtok(NULL, ",");
                clinics[clinic_index].doc_start_times[i] = atof(field);
                field = strtok(NULL, ",");
                clinics[clinic_index].doc_end_times[i] = atof(field);
            }

            // Since I do not have further information about each clinic, I asssume that all the dosctors in each clinic has the same number of patients, as well as the same start and end time.
            if (i>0)
            {
                clinics[clinic_index].num_pats[i] = clinics[clinic_index].num_pats[0];
                clinics[clinic_index].doc_start_times[i] = clinics[clinic_index].doc_start_times[0];
                clinics[clinic_index].doc_end_times[i] = clinics[clinic_index].doc_end_times[0];
            }


            people[DOCTOR][total_docs].person_type = DOCTOR;
            people[DOCTOR][total_docs].index = total_docs;
            people[DOCTOR][total_docs].clinic = clinic_index;
            people[DOCTOR][total_docs].direction = UP;
            people[DOCTOR][total_docs].current_floor = LOBBY;
            people[DOCTOR][total_docs].to_floor = clinics[clinic_index].floor;
            people[DOCTOR][total_docs].start_time = clinics[clinic_index].doc_start_times[i];
            people[DOCTOR][total_docs].end_time = clinics[clinic_index].doc_end_times[i]; 
            people[DOCTOR][total_docs].appointment_duration = clinics[clinic_index].doc_end_times[i]-clinics[clinic_index].doc_start_times[i]; // add them to the list when the want to come back 
            //sms: remember to load doctor and staff end times into event calendar for them calling an elevator from their clinic floor back down to lobby at end of day.

            total_docs++;
        }
        row++;

    }

    fclose(clinic_file);

    //READ IN ELEVATOR INFO information sheet one line at a time
    row = 0;
    while (fgets(buf, 1024, elevator_file))
    {
        if (row == 0 || row == 1)   //header row
        {
            row++;
            continue;
        }
        if (row >= NUM_ELEVATORS + 2)  //the +2 for the two header rows
            break;

        //if we are here, then we are still reading the rows of data
        char* field = strtok(buf, ",");
        for (i = 0; i < BUILDING_HOURS; i++)
        {   
            //the first strtok above just reads in the elevator index...we already know that from the row, so just do the next read
            field = strtok(NULL, ",");
            elevators[row - 2].floor_idle[i] =atoi(field);
        }
        row++;
    }

    fclose(elevator_file);
    num_docs = total_docs;
}
 
/********************************************************************************************
Initialize_Rep() resets all counters, other initial conditions at the start of each simulation replication
********************************************************************************************/
void Initialize_Rep()
{  
    // Bunch of parameters that we use for testing and debugging the code.
    tot_events=0; offload_total=0; offload_non_lobby=0; ele_assign_counter = 0; elevator_call = 0;

    my_time = -2.0;   
    lobby_arrival = 0;
    elevator_avail_lobby = 0;

    tnow = 0; //"time now"
    person_direction=NO_DIRECTION;
    person_current_floor=LOBBY;

    num_events_on_calendar = 0; // @ t=0 no one wait for elevator in any direction in any floor
    /*
    for (i = 0; i < NUM_DIRECTIONS; i++)
        for (j = 0; j < NUM_FLOORS; j++)
            people_waiting_elevator[i][j] = 0;
    */

    for (i = 0; i < NUM_FLOORS ; i++)
    {
        queue_size[UP][i] = 0;
        queue_size[DOWN][i] = 0; 
    }
    for (i = 0; i < NUM_ELEVATORS; i++) // @ t=0 all elevators are at their idle situations and their t=0 floor idle
    {   
        elevators[i].num_people = 0;
        elevators[i].idle = 1; // idle can get binary values , 1 == True
        elevators[i].idle_floor = elevators[i].floor_idle[0];
        elevators[i].current_floor = elevators[i].floor_idle[0]; //set the current floor of elevators as the idle floor assigned at time 0.
        elevators[i].final_destination = 0;
        elevators[i].elevator_time[UP] = 0;
        elevators[i].elevator_time[DOWN] = 0;
        elevators[i].direction = NO_DIRECTION;
        elevators[i].time_to_reach =999;
        elevator_head[i] = NULL; // Still no person linked to elevator
        /*
        for (j = 0; j < NUM_FLOORS; j++)
        {
            elevators[i].floor_to[j] = 0;
        }
        */  
    }

    for (i = 0; i < NUM_DIRECTIONS; i++) 
    {
        for (j = 0; j < NUM_FLOORS; j++)
        {
            //num_wait_floor[i][j] = 0;   
            hall_head[i][j] = NULL; //list of people waiting for elevators for any direction in any floor is empty now
            num_events_on_headhall = 0 ;// total number of people in line is zero
            num_events_on_elevator=0 ; // 
            //people_queue_lobby =0; // people waiting in a queue at lobby
        }
    }


    /////////////////////////////
    for (i = 0; i < NUM_CLINICS; i++)
        { 
            clinic_head[i] = NULL;
            clinics[i].num_patients = 0;
            clinic_queue_length[i] = 0;   
            clinic_queue_length[i] = 0;         
        }
    //////////////////////////////    

} //end Initialize()

/********************************************************************************************
Load_Lobby_Arrivals() loads all of the first-of-day Lobby elevator arrival events onto the event calendar.
These will be some offset based on the patients' scheduled appointment times, and the staff and doctors' scheduled start times. 
********************************************************************************************/
void Load_Lobby_Arrivals()
{   
    current_rep++;
    int i, j, k, clinic_floor;
    double minutes_before_start_time, unif, appointment_duration;
    total_pats=0, total_staff=0;

    //first, load all doctor info and their arrival times
    //note, some of the doctor structure info was loaded when we read the clinic input file
    for (i = 0; i < total_docs; i++)
    {
        minutes_before_start_time = Normal(AVG_MINS_BEFORE_APPT, SD_MINS_BEFORE_APPT); //random offset for arriving relative to appointment time
        
        people[DOCTOR][i].current_floor = LOBBY;
        people[DOCTOR][i].line_up_time=999;
        people[DOCTOR][i].elevator_wait_time[UP] = people[DOCTOR][i].elevator_wait_time[DOWN] = 0;
        people[DOCTOR][i].elevator_start_travel_time=999;
        people[DOCTOR][i].elevator_travel_time=0;
        people[DOCTOR][i].elevator_out_time=999;
        people[DOCTOR][i].elevator_ind[repnum] = 999;
        people[DOCTOR][i].get_in_clinic_time=999;    

        people[DOCTOR][i].arrive_to_elevator_time = max(people[DOCTOR][i].start_time - minutes_before_start_time, 0); //time 0 is when the doors open, so can't arrive before then
        Load_Event(&event_head, people[DOCTOR][i].arrive_to_elevator_time, PERSON_ARRIVES_ELEVATOR_HALL, DOCTOR, i); //load event for doctor into our list
    }

    //next, load all staff and first-of-day patient lobby arrival times for the day onto event calendar
    for (i = 0; i < NUM_CLINICS; i++) //loop over each clinic
    {
        clinic_floor = clinics[i].floor;
        //load staff info and their arrival times
        for (j = 0; j < clinics[i].num_staff; j++)
        {
            people[STAFF][total_staff].person_type = STAFF;   // First we want to add STAFF into our people struct
            people[STAFF][total_staff].index = total_staff;
            people[STAFF][total_staff].clinic = i;
            people[STAFF][total_staff].direction = UP;
            people[STAFF][total_staff].current_floor= LOBBY;
            people[STAFF][total_staff].to_floor = clinic_floor;
            people[STAFF][total_staff].start_time = STAFF_START_TIME;
            people[STAFF][total_staff].end_time = STAFF_END_TIME;
            people[STAFF][total_staff].appointment_duration = STAFF_END_TIME-STAFF_START_TIME; //  add to the list when they are done as departure clinic event
            minutes_before_start_time = Normal(AVG_MINS_BEFORE_APPT, SD_MINS_BEFORE_APPT); //random offset for arriving relative to appointment time
            
            people[STAFF][total_staff].current_floor=LOBBY;
            people[STAFF][total_staff].line_up_time=999.0;
            people[STAFF][total_staff].elevator_wait_time[UP] = people[STAFF][total_staff].elevator_wait_time[DOWN] = 0.0;
            people[STAFF][total_staff].elevator_start_travel_time=999.0;
            people[STAFF][total_staff].elevator_travel_time=0.0;
            people[STAFF][total_staff].elevator_out_time=999.0;
            people[STAFF][total_staff].elevator_ind[repnum] = 999;
            people[STAFF][total_staff].get_in_clinic_time=999.0;  

            people[STAFF][total_staff].arrive_to_elevator_time = max(people[STAFF][total_staff].start_time - minutes_before_start_time, 0); //time 0 is the when the doors open, so can't arrive before then
            Load_Event(&event_head, people[STAFF][total_staff].arrive_to_elevator_time, PERSON_ARRIVES_ELEVATOR_HALL, STAFF, total_staff);

            total_staff++;
        }

        //load patient info and their arrival times

        for (j = 0; j < clinics[i].num_docs; j++) //loop over each doctor working today in this clinic
        {
            //we assume for now that each patient a doctor will see that day has an appointment duration of (doc end time - doc start time)/num_pats
            appointment_duration = (clinics[i].doc_end_times[j] - clinics[i].doc_start_times[j]) / clinics[i].num_pats[j];
            for (k = 0; k < clinics[i].num_pats[j]; k++)  //loop over each patient this doc will see today in this clinic
            {    

                people[PATIENT][total_pats].person_type = PATIENT;
                people[PATIENT][total_pats].index = total_pats;
                people[PATIENT][total_pats].clinic = i;   // indexing patients
                people[PATIENT][total_pats].direction = UP;
                people[PATIENT][total_pats].current_floor= LOBBY; 
                people[PATIENT][total_pats].to_floor = clinic_floor;   //when entering lobby, the to floor is the clinic they are going to
                people[PATIENT][total_pats].start_time = clinics[i].doc_start_times[j] + k * appointment_duration; 
                people[PATIENT][total_pats].appointment_duration = appointment_duration; // assume uniform duration time for each patient
                
                people[PATIENT][total_pats].line_up_time = 999;
                people[PATIENT][total_pats].elevator_wait_time[UP] = people[PATIENT][total_pats].elevator_wait_time[DOWN] = 0;
                people[PATIENT][total_pats].elevator_start_travel_time = 999;
                people[PATIENT][total_pats].elevator_travel_time = 0;
                people[PATIENT][total_pats].elevator_out_time = 999;
                people[PATIENT][total_pats].elevator_ind[repnum] = 999;



                people[PATIENT][total_pats].appointment_wait_time = 0;
                people[PATIENT][total_pats].get_in_clinic_time = 999;

                unif = Unif(); // uniform number generates to see if patient will show up or not ! 
                //if (unif < NO_SHOW_PROB) // threshold 
                //    people[PATIENT][total_pats].no_show = 1;
                //else
                    people[PATIENT][total_pats].no_show = 0;

                //only add an arrival to the event calendar for this patient if they show up
                if (people[PATIENT][total_pats].no_show == 0) 
                {
                    minutes_before_start_time = Normal(AVG_MINS_BEFORE_APPT, SD_MINS_BEFORE_APPT); //random offset for arriving relative to appointment time
                    people[PATIENT][total_pats].arrive_to_elevator_time = max(people[PATIENT][total_pats].start_time - minutes_before_start_time, 0); //time 0 is the when the doors open, so can't arrive before then
                    Load_Event(&event_head, people[PATIENT][total_pats].arrive_to_elevator_time, PERSON_ARRIVES_ELEVATOR_HALL, PATIENT, total_pats);
                }
                total_pats++; // move on to next patient ( increment patient index )
            } //end for k looping through number of this doctors patients
        }//end for j looping through each doctor in this clinic
    }//end for i looping through each clinic``

    //sms test
    ;
    dummy = 0;

    num_staff = total_staff;
    num_patients = total_pats;
}
/********************************************************************************************
Load_Event() inserts a new event into the event calendar (a linked list), maintaining the chronological order
********************************************************************************************/
void Load_Event(struct event_node** head_ref, double time, int event_type, int entity_type, int index)
{
    struct event_node* current;
    struct event_node* event_ptr;

    event_ptr = (struct event_node*)malloc(sizeof(struct event_node));
    event_ptr->Event.time = time;
    event_ptr->Event.event_type = event_type;
    event_ptr->Event.entity_type = entity_type;
    event_ptr->Event.entity_index = index;

    /* Special case for inserting at the head  */
    if (*head_ref == NULL || (*head_ref)->Event.time >= event_ptr->Event.time)
    {
        event_ptr->next = *head_ref;
        *head_ref = event_ptr;
    }
    else
    {
        /* Locate the node before the point of insertion */   
        current = *head_ref;
        while (current->next != NULL && current->next->Event.time < event_ptr->Event.time)
            current = current->next;

        event_ptr->next = current->next;
        current->next = event_ptr;
    }
    num_events_on_calendar++;
}

/********************************************************************************************
Print_Calendar() prints all events currently in the event calendar to an external file
********************************************************************************************/
void Print_Calendar()
{
    int i;
    struct event_node* event_ptr;

    event_ptr = (struct event_node*)malloc(sizeof(struct event_node));

    for (i = 0; i < num_events_on_calendar; i++)
    {
        if (i == 0)
            event_ptr = event_head;
        else
            event_ptr = event_ptr->next;

        fprintf(output_file, "%.2f\t%d\t%d\t%d\n", event_ptr->Event.time, event_ptr->Event.event_type, event_ptr->Event.entity_type, event_ptr->Event.entity_index);
    }
    dummy = 0;
    fprintf(output_file,"End of This Calender\n");
}

/********************************************************************************************
Remove_Event() removes the head of the event calendar; i.e., deletes the event that was scheduled to occur next.
This is called after the code above already obtained that next event to process in the simulation. 
********************************************************************************************/
void Remove_Event(struct event_node** head_ref)
{
    struct event_node* temp;

    if (*head_ref == NULL)
    {
        printf("head_ref should never be NULL when calling Remove_Event\n");
        exit(0);
    }

    temp = *head_ref;
    *head_ref = temp->next;
    free(temp);
    num_events_on_calendar--;
}

/********************************************************************************************
Elevator_Available() is called when someone is trying to get on an elevator at floor "floor".  It checks
if/how many elevators are sitting idle at that floor.  If there are n > 0 elevators to choose from, we assume 
the person will choose any of them with prob 1/n each. 
Output from this function would be 0 if there is no elevator available in the same floor in idle situation
OR a number from 1 to NUM_ELEVATORS indicate which elevator is avaiable for pickup.
********************************************************************************************/
//HP: I just want to check with you on this...this function is only ever called upon arrive to hallway events. Does the code allow for the 
//possiblity that someone arrives to the hallway (say the only person needing an elevator) *during* the time an elevator on its way in the same
//direction is unloading people? I guess that's not relevant when we are only considering Lobby to floor and back to Lobby travel.  something to consider in more general case.
//sms4: the code allows for this by postponing the "send_elevator" by DOOR_TIME, and I think it is still relevant in the current version of the code.
int Elevator_Available(elevator elevs[], int floor, int direction)
{
    int i, num_avail = 0;
    double unif, equal_probs;

    //this for loop counts the number of elevators that are available to load someone on
    for (i = 0; i < NUM_ELEVATORS; i++)
    {
        if ((elevs[i].num_people < ELEVATOR_CAPACITY) && (elevs[i].current_floor == floor))
            {
                if ((elevs[i].direction == direction) || (elevs[i].idle == YES))  
                    {num_avail++;}
            }
    }

    if (num_avail == 0) // no  elevator available for loading on the same floor 
        return 0;
    else //pick one of the available ones with equal prob
    {
        equal_probs = 1.0 / num_avail;
        unif =  Unif();

        //HP: we can clean this part as it's too repetive from the loop we already did (I know i created this repeated looping below).  e.g, above we can 
        //just keep an array of the indices of available elevators, and then do some simple index = unif*num_avail (round down?) and return elevators_avail[index]
        //or something like that.  hoefully that makes some sense; we can chat if you want.  this is a minor clean-up point i'm making. 
        num_avail = 0;
        for (i = 0; i < NUM_ELEVATORS; i++)
        {
            //if ((elevs[i].idle == 1) && (elevs[i].current_floor == floor))
            if ((elevs[i].num_people < ELEVATOR_CAPACITY) && (elevs[i].current_floor == floor))
            {
                if ((elevs[i].direction== direction)||(elevs[i].idle == YES))
                    {num_avail++;}
                if (unif < num_avail * equal_probs)
                    return i + 1;  //the +1 makes it so that a "true" value (i.e., > 0) is returned even if the elevator with index 0 is chosen.
            }
        }
    }

 printf("Should never reach this point (Elevator_Available function) \n");
 exit(0);

} 

//sms: eventually tidy up code to have one Load_Event function call that handles load_event, Load_Person_Queue, and Load_Person_Elevator...there is a lot of duplication and copy/paste
// hp: can we please discuss this (Now I have changed the definition of Load_Person_Elevator function).
/********************************************************************************************
Load_Person_Queue() inserts a new event into the event calendar (a linked list), maintaining the chronological order
********************************************************************************************/
void Load_Person_Queue(struct person_node** head_ref, double time, int person_type, int index)
{
    struct person_node* current;
    struct person_node* person_ptr;

    person_ptr = (struct person_node*)malloc(sizeof(struct person_node));
    person_ptr->Person.time = time;
    person_ptr->Person.person_type = person_type;
    person_ptr->Person.index = index;

    /* Special case for inserting at the head  */
    if (*head_ref == NULL || (*head_ref)->Person.time >= person_ptr->Person.time)
    {
        person_ptr->next = *head_ref;
        *head_ref = person_ptr;
    }
    else
    {
        /* Locate the node before the point of insertion */   
        current = *head_ref;
        while (current->next != NULL && current->next->Person.time < person_ptr->Person.time)
            current = current->next;

        person_ptr->next = current->next;
        current->next = person_ptr;
    }
    num_events_on_headhall ++;
    
}

/********************************************************************************************
Remove_Event() removes the head of the event calendar; i.e., deletes the event that was scheduled to occur next.
This is called after the code above already obtained that next event to process in the simulation. 
********************************************************************************************/
void Remove_Person_Queue(struct person_node** head_ref)
{
    struct person_node* temp;

    if (*head_ref == NULL)
    {
        printf("head_ref should never be NULL when calling Remove_Event\n");
        exit(0);
    }

    temp = *head_ref;
    *head_ref = temp->next;
    free(temp);
    num_events_on_headhall--;
    
}

/********************************************************************************************
Load_Person_Elevator() inserts someone onto the elevator linked list (list of people currently traveling on this elevator)
//HP: note: we actually don't use any logic that cares if this linked list is ordered in some way..e.g., when an elevator arrives to a floor, right //sms4: I think we do, as we only consider the top of the list (line 192). But I can change it if required.
//now the code goes through everyone in the elevator to see if they are getting off.  it doesn't stop checking based on the ordering of floor_to values
//so, no logical problem, just a note for either using the ordering 
********************************************************************************************/
void Load_Person_Elevator(struct person_node** head_ref, double time, int person_type, int index, int floor_to, int direction)
{
    struct person_node* current;
    struct person_node* person_ptr;

    person_ptr = (struct person_node*)malloc(sizeof(struct person_node));
    person_ptr->Person.time = time;
    person_ptr->Person.person_type = person_type;
    person_ptr->Person.index = index;
    person_ptr->Person.to_floor = floor_to;

    if (direction==UP)  //sms2: I don't see the diff between the "if" and the "else" condition here.  can't we collapse the code? (please see the line below)
        //HP: I meant the if (UP) vs else (DOWN)...they are nearly the same pieces of code, just a >= vs. < (and to my point above, we never even use this ordering in logic yet as far as I can tell (sms4: If no one gets off in the down direction in the non0lobby floors, we can remove this.)
    {//hp: I think we can't cpllapse this. "if" statement is for the case where the linked list is empty, or the new entry deserves to be the head. "else" statement if for the case of inserting the new entry in the middle of the linked list. Please note taht the actions after each one are different.
        /* Special case for inserting at the head  */
        if (*head_ref == NULL || (*head_ref)->Person.to_floor >= person_ptr->Person.to_floor)
        {
            person_ptr->next = *head_ref;
            *head_ref = person_ptr;
        }
        else
        {
            /* Locate the node before the point of insertion */  // here we want to locate them based on their floor 
            current = *head_ref;
            while (current->next != NULL && current->next->Person.to_floor < person_ptr->Person.to_floor)
                current = current->next;

            person_ptr->next = current->next;
            current->next = person_ptr;
        }
    }
    //HP: this is what i meant is nearly identical code to the UP condition.  please collapse.
    if (direction==DOWN)
    {
        /* Special case for inserting at the head  */
        if (*head_ref == NULL || (*head_ref)->Person.to_floor <= person_ptr->Person.to_floor)
        {
            person_ptr->next = *head_ref;
            *head_ref = person_ptr;
        }
        else
        {
            /* Locate the node before the point of insertion */  // here we want to locate them based on their floor 
            current = *head_ref;
            while (current->next != NULL && current->next->Person.to_floor > person_ptr->Person.to_floor)
                current = current->next;

            person_ptr->next = current->next;
            current->next = person_ptr;
        }
    }
    num_events_on_elevator ++;  //HP: this never seems to be used for logic...
} 

/********************************************************************************************
Remove_Event() removes the head of the event calendar; i.e., deletes the event that was scheduled to occur next.
This is called after the code above already obtained that next event to process in the simulation. 
********************************************************************************************/
void Remove_Person_Elevator(struct person_node** head_ref)
{
    struct person_node* temp;

    if (*head_ref == NULL)
    {
        printf("Elevator: head_ref should never be NULL when calling Remove_Event\n");
        exit(0);
    }

    temp = *head_ref;
    *head_ref = temp->next;
    free(temp);
    num_events_on_elevator--;
}

/********************************************************************************************
Find Median
********************************************************************************************/
// Sort an array
void Array_sort(int *array , int n)
{ 
    // declare some local variables
    int i=0 , j=0 , temp=0;

    for(i=0 ; i<n ; i++)
    {
        for(j=0 ; j<n-1 ; j++)
        {
            if(array[j]>array[j+1])
            {
                temp        = array[j];
                array[j]    = array[j+1];
                array[j+1]  = temp;
            }
        }
    }
}

// function to calculate the median of the array
float Find_median(double my_array[] , int n)
{
    double temp=0;
    double array[n];
    for(i=0 ; i<n ; i++)
        array[i] = my_array[i];
    
    // First, let us sort 
    for(i=0 ; i<n ; i++)
    {
        for(j=0 ; j<n-1 ; j++)
        {
            if( array[j] > array[j+1])
            {
                temp        = array[j];
                array[j]    = array[j+1];
                array[j+1]  = temp;
            }
        }
    }
    

    // Now use the sorted array tro find the median.
    
    float median=0;
    // if number of elements are even
    if(n%2 == 0)
        median = (array[(n-1)/2] + array[n/2])/2.0;
    // if number of elements are odd
    else
        median = array[n/2];
    
    return median;
}

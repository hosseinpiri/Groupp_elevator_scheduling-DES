#pragma once

//INTEGER constants
#define NUM_FLOORS 10  //we are counting them as Lobby plus 9 floors above it.
#define LOBBY 0        //giving lobby the floor index of 0
#define NUM_CLINICS 76  //VCH clinics...still need to get non-VCH clinics using DC
#define NUM_ELEVATORS 5 //
#define MAX_STAFF_PER_CLINIC 5
#define MAX_DOCS_PER_CLINIC 8 // For now I make it 20 so that I dod not get any errors in stress testing.
#define MAX_PATS_PER_DOC 8
#define MAX_PEOPLE  (NUM_CLINICS*MAX_DOCS_PER_CLINIC*MAX_PATS_PER_DOC) //2240; the system will have more possible patients than docs or staff
#define ELEVATOR_CAPACITY 4 //made up
#define BUILDING_HOURS 12   
#define BOGUS_VALUE -999
#define NUM_DIRECTIONS 2 //one for up, one for down...obvious that there are two, this is just to make the code more readable in places instead of using [2] we'll use [NUM_DIRECTIONS] for array declarations, so that we don't have to think what the 2 represents. 
#define UP 0
#define DOWN 1
#define NO_DIRECTION 3
#define NO 0
#define YES 1
#define EVENT_ENTRIES (MAX_PEOPLE*3)  //the *3 is just to more than cover need. MAX_APPTS*NUM_CLINICS is the max # of patients we are scheduling with arrival times on the event calendar.  will eventually add their departure times to event calendar.  other events like end of simulation.  prob 2* is enough. 
#define CLINIC_TO_ELEVATOR_TIME 10.0/60.0 // Time that it takes to go from elevator to clinic or vice versa

//EVENT types
#define PERSON_ARRIVES_ELEVATOR_HALL 1
#define ELEVATOR_ARRIVAL 2
#define CLINIC_ARRIVAL 3  
#define CLINIC_COMPLETION 4
#define SEND_ELEVATOR 5
#define ELEVATOR_REQUEST 6
//#define PERSON_ARRIVES_LOBBY 7

//PERSON types
#define NUM_PEOPLE_TYPES 3
#define PATIENT 0
#define STAFF 1
#define DOCTOR 2
//Another entity Type
#define ELEVATOR 3

//FLOATING POINT constants
#define EPSILON 0.000001 // for event list
#define ELEVATOR_VELOCITY 500.0  //feet per minute // This info is real from the emails.
#define FLOOR_DISTANCE 14.0 //distance betweeen consecutive floors, in feet
#define ELEVATOR_DOOR_OPEN_TIME 5.0/60 //in minutes...I'm assuming 5 seconds...we'll use this for door close time too 
#define PERSON_LOAD_TIME 5.0/60 //in minutes...I'm assuming 5 seconds
#define HOURS_OPEN 12.0
#define STAFF_START_TIME 0 //for now i'm just assuming staff start at start of day (minute 0) and finish 8 hours later
#define STAFF_END_TIME 480
#define AVG_MINS_BEFORE_APPT 15.0
#define SD_MINS_BEFORE_APPT 0.00  //0 to start with a determnistic model to make debugging easier
#define FACILITY_END_TIME (HOURS_OPEN*60.0)
#define NO_SHOW_PROB 0.8  //0 to start with deterministic model
#define DOOR_TIME 5.0/60.0 // Time for each door oper OR door close. 

//SIMULATION RUN parameters
#define REPS 100  //1 for now for debugging...crank this up later

//STRUCTURES
typedef struct
{
    int person_type; //PATIENT, STAFF, OR DOCTOR
    int index; //index of patient array this patient will occupy
    int clinic;
    int current_floor;
    int to_floor;
    int direction;
    double start_time;  //for patients, this is the scheduled appointment start time; for staff/docs, this is their working start time
    double arrive_to_elevator_time;
    double line_up_time;
    double clinic_line_up_time;
    double elevator_wait_time[NUM_DIRECTIONS];
    double elevator_start_travel_time;
    double elevator_travel_time;
    double elevator_out_time;
    int elevator_ind[REPS]; // elevator index that a person move with
    double time; // gonna define
    //double elevator_wait_time_down;
    double get_in_clinic_time;

    //these are only relevant if person_type is PATIENT
    int num_visitors; //indicates how many other people (e.g., family) are with this patient.
    int no_show; // = 1 indicates patient doesn't show up for appointment. 
    double appointment_wait_time;
    double appointment_duration; // appointment duration
    //this is only relevant if type is STAFF or DOC
    double end_time; //their planned minutes from 0 to end their shift
} person;

typedef struct 
{
    int current_floor;
    int next_floor;
    int num_people;
    double elevator_time[2]; // 0: elevator going up , 1 : elevator going down 
    int direction;
    int idle_floor;
    int idle;  //binary; 1 = true
    int floor_idle[BUILDING_HOURS]; //indicates which floor this elevator should idel at by hour of day. 
    //int floor_to[NUM_FLOORS];  //1 if a button is pressed inside to stop at that floor or elevator is assigned to stop at that floor for a hall pickup
    //int person_type[ELEVATOR_CAPACITY]; //indicates whether each person on the elevator is a patient, doctor, or staff
    //int person_index[ELEVATOR_CAPACITY];  // indicates person index of that type 
    //double elevator_up_time;
    //double elevator_down_time;
    double time_to_reach;
    int final_destination; 
} elevator;

typedef struct
{
    int index;
    int floor;  //i'm assuming lobby is entered as floor 0, and that one storey above ground is called floor 1, etc.  
    int num_staff;
    int num_docs;
    int num_patients;
    int num_pats[MAX_DOCS_PER_CLINIC];   
    double doc_start_times[MAX_DOCS_PER_CLINIC];
    double doc_end_times[MAX_DOCS_PER_CLINIC];
} clinic;

typedef struct 
{
    double time;        //minutesp
    int event_type;     //LOBBY ARRIVAL, ELEVATOR ARRIVAL, ETC. 
    int entity_type;    //PATIENT, STAFF, DOCTOR, ELEVATOR
    int entity_index;   //if type is pat, staff, or doc, then this will be the index of that array that holds their info
    int elevator_num;   // used for ELEVATOR ARRIVAL event              
                        //if type is elevator, then this is the elevator index
} event;

struct event_node
{
    event Event;        
    struct event_node* next;
} ;  //for creating event calendar as a linked list 

struct person_node
{
    person Person;
    struct person_node* next;
};  //for creating event calendar as a linked list 
#ifndef _STATISTICS_H_
#define _STATISTICS_H_

int num_docs, num_staff, num_patients;


typedef struct
{
    int current_rep, total_reps;

    int num_patients;
    int num_docs;
    int num_staff;
    int total_num_people;
	
    
    double average_elevator_in_use_time;
    double elevator_utilization[NUM_ELEVATORS];
	double average_elevator_utilization ;
	double average_elevator_utilization_by_hour[BUILDING_HOURS];

	// People parameters
    double average_elevator_wait_doc_staff;
	double average_elevator_wait_patient;
    double average_elevator_wait_all_people;

    double median_elevator_wait_doc_staff;
	double median_elevator_wait_patient;

	double max_elevator_wait_doc_staff;
	double max_elevator_wait_patient;
    double max_elevator_wait_all_people;

	double max_elevator_wait_by_floor_patient[NUM_FLOORS];

	double average_elevator_travel_time ;
	double max_elevator_travel_time ;
	
    double average_appointment_wait;
	double max_appointment_wait;
    //double average_clinic_queue_length;
    int  max_clinic_queue_length;

	//double average_hall_queue_size[NUM_FLOORS];
	//double average_hall_queue_size_across_floors;

    double max_hall_queue_size[NUM_FLOORS];

	int count_patient_to_floors[NUM_FLOORS];
	int count_doc_staff_to_floors[NUM_FLOORS];
} statistics;
#endif

//sms: put these into a statistics structure // hp (sms): can we please discuss this. I am not sure if I understood what you mean.
double elevator_wait_time[NUM_DIRECTIONS];  //we will record this for both patients and staff, where the index 2 is for the UP vs. DOWN direction
double elevator_travel_time[NUM_DIRECTIONS]; 

//GLOBAL variables/arrays
int current_rep;
int clinic_num;
int people_waiting_elevator[NUM_DIRECTIONS][NUM_FLOORS];
int total_pats=0, total_staff=0, total_docs=0;  //these are totals for the day, for knowing the actual max used index of the structure arrays below
int num_events_on_calendar, dummy, door_open;
double tnow;
double time_per_floor = FLOOR_DISTANCE / ELEVATOR_VELOCITY; //minutes
double elevator_travel_time_per_floor = FLOOR_DISTANCE / ELEVATOR_VELOCITY; //minutes
int onload, offload;
char str[4];
int num_people_for_animation, direction_for_animation;

struct person_node* person_ptr; 
struct event_node* event_head; //starts the linked list for event calendar
struct person_node* elevator_head[NUM_ELEVATORS]; //a linked list of people for each elevator
struct person_node* hall_head[NUM_DIRECTIONS][NUM_FLOORS+1]; //1(lobby)+NUM_FLOORS a linked list of people waiting for elevators in each direction
struct person_node* clinic_head[NUM_CLINICS];
clinic clinics[NUM_CLINICS];
person people[NUM_PEOPLE_TYPES][MAX_PEOPLE]; //keeps a record of all patients, staff, and doctors that go through a simulation replication.  // hp: At each replication, we call the function Load_Lobby_Arrivals(), which updates people[NUM_PEOPLE_TYPES][MAX_PEOPLE].
elevator elevators[NUM_ELEVATORS];

//HEADERS for functions
void Open_and_Read_Files();
void Load_Event(struct event_node**, double, int, int, int);
void Load_Person_Queue(struct person_node**, double,int, int);
void Load_Person_Elevator(struct person_node**, double ,int , int ,int, int);
void Remove_Person_Elevator(struct person_node**);
void Remove_Event(struct event_node**);
void Remove_Person_Queue(struct person_node**);
void Load_Lobby_Arrivals();
void Initialize_Rep();
void Print_Calendar();
void Send_Elevator(int index , event next_event) ;
int Elevator_Available(elevator elevs[], int floor, int direction);
void initialize_vars(statistics* stats);
void print_results(statistics* stats, int total_reps);
void help_print_results(double vector[REPS]);

void Array_sort(int *array , int n);
float Find_median(double array[] , int n);

//sms: these should already be taken care of in the stdlib.h // hp(sms): I am getting error with removing these.
#define max(x, y) (((x) > (y)) ? (x) : (y))

#define min(x, y) (((x) < (y)) ? (x) : (y))

#define abs(x) (((x) > (0)) ? (x) : (-1*x))

int tot_events, offload_total, offload_non_lobby, ele_assign_counter , elevator_call ;
int my_type,time_index, i, ii , my_index , direction_indicator , lobby_arrival , elevator_avail_lobby , j, keepgoing, repnum, elevator_avail, person_to_floor, elevator_index;
double minutes_before_appt_time, unif;
double wait_time, my_time;
double prob;
double Z95, RCI, LCI;
int ele_index;
int num_ele;
int num_events_on_headhall ;
int num_events_on_elevator;
int direction;
int storey, counter;

person next_in_Line;
person my_person;
person next_in_clinic_queue;
int people_queue_lobby; 
double wait_time_vec[NUM_ELEVATORS];

person next_in_elevator;
int num_clinic_filles;
int person_to_floor, person_current_floor ;
double time_to_reach;
int distance;
double earliest_request;
int elevator_direction, person_direction;
double travel_time; 
double average_clinic_queue_length; 
int queue_size[2][NUM_FLOORS];
//double queue_registeration_time[NUM_FLOORS];
int clinic_queue_length[NUM_CLINICS];  
int counter;
int storey;
int last_indicator, indicator, multiplier;
int number;
int p;
int idle_floor;
int coun;
int to_floor;
int totl ;
int person_out_clinic;
int ind;
int tot_num_ele;
int k;
int var;



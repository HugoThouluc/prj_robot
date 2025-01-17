#include "main.h"
#include "motorCommand.h"
#include "quadEncoder.h"
#include "captDistIR.h"
#include "VL53L0X.h"
#include "groveLCD.h"

#define ARRAY_LEN 100
#define SAMPLING_PERIOD_ms 5
#define FIND_MOTOR_INIT_POS 0
#define DEFAULT_SPEED 500
//################################################
#define EX1 1
#define EX2 2
#define EX3 3

#define SYNCHRO_EX EX1
//int tab_speed1[200];
//int tab_speed[200];
int vl_seuil=100;
int ir_seuil=50;
//################################################
//reglage du moteur
#define Kp 0.01
#define Te 5
#define tau 30
#define Ti (0.1*tau)
#define Ki (Te/Ti)
#define offset	100  // valeur de repos du moteur


//################################################
// PARAMETRE A MODIFIER EN FONCTION DU N° ROBOT
#define ROS_DOMAIN_ID 1
//################################################
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

extern I2C_HandleTypeDef hi2c1;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 3000 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};


struct AMessage // Exemple de type de message pour queues, on peut mettre ce qu'on veut
{
	char command;
	int data;
};
/**** modes de fonctionnement **/
enum Working_mode {
	MANUEL, AUTOMATIQUE, TRACKING
};

enum Working_mode current_mode = MANUEL;
int cmd_manuel = 0;
/**** mouvement lors des deplacements **/
enum Motion {
	avant ,arrière, Turning_left, Turning_right, Stop
};
/***** Structure pour l'envoi des flags des capteurs de distance **/
struct Dist_Flags {
	int AVD;
	int AVG;
	int ARR;
};

/***** Message pour passer la vitesse **/
struct MotorSpeed {
	char motor;
	int speed;
};
int tab_speed[100];
int tab_speed1[100];
int consigne=500;
int consigneSpeedLeft = 0, consigneSpeedRight = 0;
enum Motion current_motion =avant;
enum Motion next_motion =avant;
void SystemClock_Config(void);
void microros_task(void *argument);
uint16_t mes_vl53=0;

int Left_first_index_reached = 0;
int Right_first_index_reached = 0;
int16_t Auto_consigne = 1000;
int16_t vitesse=0;

//========================================================================
#if SYNCHRO_EX == EX1

int tab[100];

/*static void task_moteur_test(void *pvParameters)
{
	int i=0;
	int speed;


	for (;;)
	{
		motorLeft_SetDuty(150);
		speed=quadEncoder_GetSpeedL();


		if(i<100){
			tab[i]=speed;
			i++;
		}

		vTaskDelay(5); // 1000 ms
	}
}*/
/*
static void task_capteur(void *pvParameters)
{
uint16_t dist_vl53;
int tab_dist[2];
int obstacle_left=0;
int obstacle_right=0;
int obstacle_arrière=0;
	for (;;)
	{
		captDistIR_Get(tab_dist);
		dist_vl53 = VL53L0X_readRangeContinuousMillimeters();
		printf("capteur vl53= %d \n\r",dist_vl53);
		printf("capteur ir left= %d \n\r",tab_dist[0]);
		printf("capteur ir right= %d \n\r",tab_dist[1]);

		if(dist_vl53<=vl_seuil)
		{
			obstacle_arrière=1;
			printf("avancer\n\r");
			//comment avancer
		}
		else if (tab_dist[0]<=ir_seuil)
		{
			obstacle_left=1;
			printf("tourner à droite \n\r");
			//comment tourner à droite
		}
		else if (tab_dist[1]<=ir_seuil)
		{
			obstacle_right=1;
			printf("tourner à gauche \n\r");
			//comment tourner à gauche
		}

		vTaskDelay(100);
	}
}*/

static void task_Asservissement_Moteur_Left(void *pvParameters)	// asservissement du moteur gauche
{
	int i = 0;
	int speed = 0;
	int erreur = 0;
	int commande = 0;
	float ui = 0.0;
	float up = 0.0;

	for (;;) {

			speed = quadEncoder_GetSpeedL();
			erreur = consigneSpeedLeft - speed;
			up = Kp * (float) erreur;
			ui = ui + Kp * Ki * (float) erreur;
			commande = (int) up + ui;

			motorLeft_SetDuty(commande + 100);

			if (i < 100) {
				 tab_speed[i] = speed;		// DEBUG ASSERVISSEMENT
				 i++;
			 }


			vTaskDelay(5);

	}
}

static void task_Asservissement_Moteur_Right(void *pvParameters)// asservissement du moteur droit
{
	int i = 0;
	int speed = 0;
	int erreur = 0;
	int commande = 0;
	float ui = 0.0;
	float up = 0.0;

	for (;;) {
			speed = quadEncoder_GetSpeedR();
			erreur = consigneSpeedRight - speed;
			up = Kp * (float) erreur;
			ui = ui + Kp * Ki * (float) erreur;
			commande = (int) up + ui;

			motorRight_SetDuty(commande + 100);

			if (i < 100) {
			 tab_speed1[i] = speed;		/// ASSERVISSEMENT
			 i++;
			 }
			vTaskDelay(5);
	}
}

static void task_led(void *pvParameters)
{
	for (;;) {
			//groveLCD_clear();
			groveLCD_setCursor(0, 0);
			switch (current_mode) {
			case MANUEL:
				groveLCD_term_printf("MODE MANUEL");
				break;
			case AUTOMATIQUE:
				groveLCD_term_printf("MODE AUTOMATIQUE");
				break;
			case TRACKING:
				groveLCD_term_printf("MODE TRACKING");
				break;
			default://command move
				break;
			}
			vTaskDelay(500);
		}
}
static void task_decision(void *pvParameters)
{

volatile static int cmd_manuel = 1;
							// 0. stop
							// 1. avant
							// 2. arrière
							// 3. gaucge
							// 4. droite

volatile int Avant_Droit = 0;
volatile int Avant_Gauche = 0;
volatile int ARR = 0;
	//////////////////////////////////////////////
int tab_dist[2];
uint16_t dist_ARR = 0;
static int dist_AVD, dist_AVG;

	vTaskDelay(5);
for (;;)
	{
/*	captDistIR_Get(tab_dist);
			dist_AVD = tab_dist[0];
			dist_AVG = tab_dist[1];
			dist_ARR = VL53L0X_readRangeContinuousMillimeters();
			mes_vl53 = dist_ARR;

			if (dist_ARR < 100) 		// si distance arriere < 2cm, on s'arrete
				ARR = 1;
			else
				ARR = 0;

			if (dist_AVD > 2000)
				Avant_Droit = 1;
			else
				Avant_Droit= 0;

			if (dist_AVG > 2000)
				Avant_Gauche = 1;
			else
				Avant_Gauche = 0;
*/
	switch (current_mode) {
	case MANUEL:
		switch (cmd_manuel) {
		case 0:
			// Stop
			consigneSpeedLeft = 0;
			consigneSpeedRight = 0;
			break;
		case 1:
			// avance
			consigneSpeedLeft = consigne;
			consigneSpeedRight = consigne;
			break;
		case 2:
			// reculer
			consigneSpeedLeft = -1 * consigne;
			consigneSpeedRight = -1 * consigne;
			break;
		case 3:
			// gauche
			consigneSpeedLeft = consigne / 10;
			consigneSpeedRight = consigne;
			break;
		case 4:
			// droite
			consigneSpeedLeft = consigne;
			consigneSpeedRight = consigne / 10;
			break;
		default:
			break;
		}
		break;

		case AUTOMATIQUE:
			current_motion = next_motion;
			switch (current_motion) {
			case avant:
				consigneSpeedLeft = consigne;
				consigneSpeedRight = consigne;
				if (Avant_Droit == 1 && Avant_Gauche == 0)
					next_motion = Turning_left;
				else if (Avant_Droit == 0 && Avant_Gauche == 1)
					next_motion = Turning_right;
				else if (Avant_Droit == 1 && Avant_Gauche == 1)
					next_motion = arrière;
				else if (Avant_Droit == 1 && Avant_Gauche == 1 && ARR == 1)
					next_motion = Stop;
				break;
			case arrière:
				consigneSpeedLeft = -1 * consigne;
				consigneSpeedRight = -1 * consigne;
				if (ARR == 1)
					next_motion = Stop;
				else if (Avant_Droit == 0 && Avant_Gauche == 0)
					next_motion =avant;
				break;
			case Turning_left:
				consigneSpeedLeft = consigne / 10;
				consigneSpeedRight = consigne;
				if (Avant_Droit == 0 && Avant_Gauche == 0)
					next_motion =avant;
				else if (Avant_Droit == 0 && Avant_Gauche == 1)
					next_motion = Turning_right;
				else if (Avant_Droit == 1 && Avant_Gauche == 1)
					next_motion = arrière;
				break;
			case Turning_right:
				consigneSpeedLeft = consigne;
				consigneSpeedRight = consigne / 10;
				if (Avant_Droit == 0 && Avant_Gauche == 0)
					next_motion = avant;
				else if (Avant_Droit == 1 && Avant_Gauche == 0)
					next_motion = Turning_left;
				else if (Avant_Droit == 1 && Avant_Gauche == 1)
					next_motion =arrière;
				break;
			case Stop:
				consigneSpeedLeft = 0;
				consigneSpeedRight = 0;
				if (Avant_Droit == 0 && Avant_Gauche == 0)
					next_motion = avant;
				else if (Avant_Droit == 1 && Avant_Gauche == 0)
					next_motion = Turning_left;
				else if (Avant_Droit == 0 && Avant_Gauche == 1)
					next_motion = Turning_right;
				else if (ARR == 0)
					next_motion = arrière;
				break;
			default:
				break;
			}
			break;

			case TRACKING:
				break;
			default:
				break;
		}

	vTaskDelay(5);
	}

}

//========================================================
#elif SYNCHRO_EX == EX2

static void task_C( void *pvParameters )
{
	for (;;)
	{
		 printf("TASK C \n\r");
		xSemaphoreTake( xSemaphore, portMAX_DELAY );
	}
}

static void task_D( void *pvParameters )
{
	for (;;)
	{
		 printf("TASK D \n\r");
		xSemaphoreGive( xSemaphore );
	}
}

//========================================================
#elif SYNCHRO_EX == EX3

static void task_E( void *pvParameters )
{
	struct AMessage pxMessage;
	pxMessage.command='a';
	pxMessage.data=10;
	vTaskDelay(1000);
	for (;;)
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 1);
	    printf("TASK E \r\n");
		xQueueSend( qh, ( void * ) &pxMessage,  portMAX_DELAY );
		xSemaphoreTake( xSemaphore, portMAX_DELAY );

		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 0);
		vTaskDelay(SAMPLING_PERIOD_ms);
	}
}

static void task_F(void *pvParameters)
{
	struct AMessage pxRxedMessage;

	for (;;)
	{
		xQueueReceive( qh,  &( pxRxedMessage ) , portMAX_DELAY );
		 printf("TASK F \r\n");
		xSemaphoreGive( xSemaphore );
	}
}
#endif


//=========================================================================
int main(void)
{
  int ret=0;
  int tab_dist[2];

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();

  motorCommand_Init();
  quadEncoder_Init();
  captDistIR_Init();
  uint16_t dist1, dist2;

  HAL_Delay(500);


  // Affichage via UART2 sur Terminal série $ minicom -D /dev/ttyACM0
  printf("Bonjour\r\n"); // REM : ne pas oublier le \n

  VL53L0X_init();

  ret = VL53L0X_validateInterface();
  if(ret ==0)
  {
	  printf("VL53L0X OK\r\n");
  }
  else
  {
	  printf("!! PROBLEME VL53L0X !!\r\n");
  }
  VL53L0X_startContinuous(0);

  int a, b;
  groveLCD_begin(16,2,0); // !! cette fonction prend du temps
  HAL_Delay(100);
  groveLCD_display();
  a=5; b=2;
  groveLCD_term_printf("%d+%d=%d",a,b,a+b);
  groveLCD_setCursor(0,0);
 // groveLCD_term_printf("bonjour");


  HAL_Delay(500);


#if FIND_MOTOR_INIT_POS

  int16_t speed=0;
// RECHERCHE DE LA POSITION INITIALE ( 1er signal 'index' du capteur )
// Evite une erreur pour une mesure de vitesse
	Left_first_index_reached = 0;
	 while( Left_first_index_reached != 1 )
	 {
		motorLeft_SetDuty(130);
		HAL_Delay(SAMPLING_PERIOD_ms);
		speed = quadEncoder_GetSpeedL();
	 }

	Right_first_index_reached = 0;
	 while( Right_first_index_reached != 1 )
	 {
		motorRight_SetDuty(130);
		HAL_Delay(SAMPLING_PERIOD_ms);
		speed = quadEncoder_GetSpeedR();
	 }

	 motorLeft_SetDuty(100);
	 motorRight_SetDuty(100);
	 HAL_Delay(200);

	 speed = quadEncoder_GetSpeedL();
	 speed = quadEncoder_GetSpeedR();
#endif

  osKernelInitialize();

  xTaskCreate( microros_task, ( const portCHAR * ) "microros_task", 2800 /* stack size */, NULL,  24, NULL );
#if SYNCHRO_EX == EX1
	//xTaskCreate( task_capteur, ( const portCHAR * ) "task capteur", 128 /* stack size */, NULL, 26, NULL );
   // xTaskCreate( task_moteur_test, ( const portCHAR * ) "task_moteur_test", 128 /* stack size */, NULL, 25, NULL );
	xTaskCreate( task_Asservissement_Moteur_Right, ( const portCHAR * ) "task_Asservissement_Moteur_Right", 256 /* stack size */, NULL, 25, NULL );
	xTaskCreate( task_Asservissement_Moteur_Left, ( const portCHAR * ) "task_Asservissement_Moteur_Left", 256 /* stack size */, NULL, 24, NULL );
	xTaskCreate( task_decision, ( const portCHAR * ) "task decision", 256 /* stack size */, NULL, 30, NULL );
    xTaskCreate( task_led, ( const portCHAR * ) "task led", 128 /* stack size */, NULL, 23, NULL );
#elif SYNCHRO_EX == EX2
	xTaskCreate( task_C, ( signed portCHAR * ) "task C", 128 /* stack size */, NULL, 28, NULL );
	xTaskCreate( task_D, ( signed portCHAR * ) "task D", 128 /* stack size */, NULL, 27, NULL );
#elif SYNCHRO_EX == EX3
	xTaskCreate( task_E, ( signed portCHAR * ) "task E", 128 /* stack size */, NULL, 30, NULL );
	xTaskCreate( task_F, ( signed portCHAR * ) "task F", 128 /* stack size */, NULL, 29, NULL );
#endif

	/*vSemaphoreCreateBinary(xSemaphore);
	xSemaphoreTake( xSemaphore, portMAX_DELAY );

	qh = xQueueCreate( 1, sizeof(struct AMessage ) );
*/
  osKernelStart();

  while (1)
  {}

}
//=========================================================================
bool cubemx_transport_open(struct uxrCustomTransport * transport);
bool cubemx_transport_close(struct uxrCustomTransport * transport);
size_t cubemx_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err);
size_t cubemx_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);

void * microros_allocate(size_t size, void * state);
void microros_deallocate(void * pointer, void * state);
void * microros_reallocate(void * pointer, size_t size, void * state);
void * microros_zero_allocate(size_t number_of_elements, size_t size_of_element, void * state);


void subscription_callback(const void * msgin)
{
  const std_msgs__msg__String * msg = (const std_msgs__msg__String *)msgin;
  if (strcmp(msg->data.data, "m") == 0) {
	  current_mode = MANUEL;
  } else if (strcmp(msg->data.data, "a") == 0) {
	  current_mode = AUTOMATIQUE;
  } else if (strcmp(msg->data.data, "t") == 0) {
	  current_mode = TRACKING;
  } else if (strcmp(msg->data.data, "0") == 0) {
	  cmd_manuel = 0;
  } else if (strcmp(msg->data.data, "1") == 0) {
	  cmd_manuel = 1;
  } else if (strcmp(msg->data.data, "2") == 0) {
	  cmd_manuel = 2;
  } else if (strcmp(msg->data.data, "3") == 0) {
	  cmd_manuel = 3;
  } else if (strcmp(msg->data.data, "4") == 0) {
	  cmd_manuel = 4;
  }
  // Process message
  printf("Received from HOST: %s\n\r", msg->data);
}

//command/move
void microros_task(void *argument)
{
  rmw_uros_set_custom_transport( true, (void *) &huart1, cubemx_transport_open,  cubemx_transport_close,  cubemx_transport_write, cubemx_transport_read);

  rcl_allocator_t freeRTOS_allocator = rcutils_get_zero_initialized_allocator();
  freeRTOS_allocator.allocate = microros_allocate;
  freeRTOS_allocator.deallocate = microros_deallocate;
  freeRTOS_allocator.reallocate = microros_reallocate;
  freeRTOS_allocator.zero_allocate =  microros_zero_allocate;

  if (!rcutils_set_default_allocator(&freeRTOS_allocator)) {
      printf("Error on default allocators (line %d)\n", __LINE__);
  }

  // micro-ROS app
  rclc_support_t support;
  rcl_allocator_t allocator;
  allocator = rcl_get_default_allocator();

  // create node
  rcl_node_t node;
  rcl_node_options_t node_ops = rcl_node_get_default_options();
  rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
  rcl_init_options_init(&init_options, allocator);
  rcl_init_options_set_domain_id(&init_options, ROS_DOMAIN_ID);

  rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator);
  rclc_node_init_default(&node, "STM32_Node","", &support);

  // create publisher
  rcl_publisher_t publisher;
  std_msgs__msg__String sensor_dist_back_msg;
  rclc_publisher_init_default(&publisher, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),"/sensor/dist_back");

  // create subscriber
  rcl_subscription_t subscriber;
  std_msgs__msg__String str_msg;
  rclc_subscription_init_default(&subscriber,&node,ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),"/command/move");

  rcl_subscription_t command_subscriber;
  std_msgs__msg__String str_msg_command;
  rclc_subscription_init_default(&command_subscriber,&node,ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),"pyqt_topic_send");

  // Add subscription to the executor
  rclc_executor_t executor;
  rclc_executor_init(&executor, &support.context, 1, &allocator); // ! 'NUMBER OF HANDLES' A MODIFIER EN FONCTION DU NOMBRE DE TOPICS
  rclc_executor_add_subscription(&executor, &command_subscriber, &str_msg_command, &subscription_callback, ON_NEW_DATA);



  str_msg.data.data = (char * ) malloc(ARRAY_LEN * sizeof(char));
  str_msg.data.size = 0;
  str_msg.data.capacity = ARRAY_LEN;

  str_msg_command.data.data = (char * ) malloc(ARRAY_LEN * sizeof(char));
  str_msg_command.data.size = 0;
  str_msg_command.data.capacity = ARRAY_LEN;

  for(;;)
  {
	  sprintf(str_msg.data.data, "from STM32 : mes_vl53 : #%d", (int32_t)mes_vl53);
	  str_msg.data.size = strlen(str_msg.data.data);//command move

      rcl_ret_t ret = rcl_publish(&publisher, &str_msg, NULL);

		if (ret != RCL_RET_OK)
		{
		  printf("Error publishing (line %d)\n\r", __LINE__);
		}
    rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
    osDelay(100);
  }
}
//=========================================================================
/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
}
//=========================================================================
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {}
}
//=========================================================================
#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

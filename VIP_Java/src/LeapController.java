import java.io.IOException;

import Serial.*;

import com.leapmotion.leap.*;

import java.util.Scanner;

class LeapListener extends Listener {

	int Flag = 0;

	SerialCom Conect = new SerialCom();

	public void Conect_Serial() {

		@SuppressWarnings("resource")
		Scanner Teclado = new Scanner(System.in);

		String PortaCOM;

		System.out.println("\nDigite a Porta COM que será utilizada !\n");

		PortaCOM = Teclado.next();

		try {
			Conect.connect(PortaCOM.toUpperCase());
			System.out.println("\nConectado a Porta Serial: "
					+ PortaCOM.toUpperCase());
		} catch (Exception e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
		}
	}

	public void onInit(Controller controller) {
		System.out.println("\nInicializando LeapMotion..");

	}

	public void onConnect(Controller controller) {

		Conect.writetoport("k");

		System.out.println("\nConectado ao LeapMotion");

		System.out.println("\nAguardando Gestos.......");

		controller.enableGesture(Gesture.Type.TYPE_SWIPE);
		// controller.enableGesture(Gesture.Type.TYPE_CIRCLE);
		// controller.enableGesture(Gesture.Type.TYPE_SCREEN_TAP);
		// controller.enableGesture(Gesture.Type.TYPE_KEY_TAP);
		controller.enableGesture(Gesture.Type.TYPE_CIRCLE);

		controller.config().setFloat("Gesture.Swipe.MinLength", 175.0f);
		controller.config().setFloat("Gesture.Swipe.MinVelocity", 320f);

		controller.config().setFloat("Gesture.Circle.MinRadius", 50.0f);
		controller.config().setFloat("Gesture.Circle.MinArc", 5f);

		controller.config().setFloat("Gesture.ScreenTap.MinForwardVelocity",
				25.0f);
		controller.config().setFloat("Gesture.ScreenTap.HistorySeconds", 1f);
		controller.config().setFloat("Gesture.ScreenTap.MinDistance", 30.0f);
		controller.config().save();

	}

	public void onDisconnect(Controller controller) {
		System.out.println("\nDesconectado");
	}

	public void onExit(Controller controller) {
		System.out.println("\nSoftware Finalizado !");
	}

	public void onFrame(Controller controller) {

		char Envio = ' ';

		Frame frame = controller.frame();

		for (Gesture gesture : frame.gestures()) {

			// Escreve no prompt o gesto que foi reconhecido
			//System.out.println(gesture.type());
			// Caso o gesto seja SWIPE, armazena na variável que será enviada
			if (gesture.type().toString() == "TYPE_SWIPE") {
				
				//System.out.println(gesture.type());

				SwipeGesture vector = new SwipeGesture(gesture);
				
				double eixox = vector.direction().getX();

				if ((eixox <= -0.68) && (eixox >= -1)){
					
					System.out.println(vector.direction());
					System.out.println("SWIPE PARA ESQUERDA\n");
					
					Envio = 'L';
		
				} else if ((eixox >= 0.70) && (eixox <= 1)) {
					
					System.out.println(vector.direction());
					System.out.println("SWIPE PARA DIREITA\n");
					
					Envio = 'R';
				}
				
				// Caso o gesto seja CIRCLE, armazena na variável que será
				// enviada
			} else if (gesture.type().toString() == "TYPE_CIRCLE") {
				
				System.out.println(gesture.type());
				
				Envio = 'C';
			}
	
			// Chama a função para escrever na Serial
			write(Envio);
			
			// Faz a tentativa de delay, tempo em ms
			try {
				Thread.sleep(1500); // 1200 milliseconds
			} catch (InterruptedException ex) {
				Thread.currentThread().interrupt();
			}
		}
	}

	public void write(char Envio) {

		if (Envio == 'S') {
			Conect.writetoport("S");
		} else if (Envio == 'C') {
			Conect.writetoport("C");
		}else if(Envio == 'L'){
			Conect.writetoport("L");
		}else if(Envio == 'R'){
			Conect.writetoport("R");
		}
	}
}

public class LeapController {

	public static void main(String[] args) {

		System.out.println("Software VIP Inicializado !");

		// Armazena dado de entrada do Sistema
		int entrada = 0;

		LeapListener listener = new LeapListener();
		Controller controller = new Controller();

		listener.Conect_Serial();

		controller.addListener(listener);

		System.out.println("\nDigite exit para Encerrar o Software !\n");

		// Enquanto o usuário não digitar exit, o software continua o processo
		while (entrada != 101) {
			try {
				// Aguarda a entrada de dado no prompt do java
				entrada = System.in.read();
			} catch (IOException e) {
				e.printStackTrace();
			}

			// Caso a entrada seja exit, finaliza o software
			if (entrada == 101) {
				// Exibe comunicação encerrada
				System.out.println("\nComunicação encerrada !");
				// Desconecta Leap Motion e a comunicação Serial
				controller.removeListener(listener);
				// Finaliza os processos do software
				System.exit(1);
			}
		}
	}
}

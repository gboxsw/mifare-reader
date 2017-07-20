package com.gboxsw.tools.mifareeditor;

import java.util.Arrays;
import java.util.Scanner;

import com.gboxsw.acpmod.gep.TCPSocket;
import com.gboxsw.arduino.mifarereader.*;

public class App {
	public static void main(String[] args) {
		CardReader reader = new CardReader(new TCPSocket("127.0.0.1", 4444));
		reader.addCardListener((cardReader, cardPresent) -> {
			if (cardPresent) {
				System.out.println("Card detected");
				System.out.println(cardReader.getCardType() + " with " + cardReader.getBlockCount() + "blocks.");
				System.out
						.println("UID: " + Arrays.toString(ArrayConverter.convertToIntArray(cardReader.getCardUID())));
			} else {
				System.out.println("Card removed.");
			}
		});

		reader.start();

		CardScriptExecutor executor = new CardScriptExecutor(reader);
		try (Scanner s = new Scanner(System.in)) {
			while (true) {
				String command = s.nextLine().trim();
				if ("exit".equals(command)) {
					break;
				}

				if ("reset".equals(command)) {
					reader.resetCard();
					continue;
				}

				StringBuilder commandOutput = new StringBuilder();
				if (!executor.execute(command, commandOutput)) {
					System.err.println("Command failed.");
				} else {
					System.out.println(commandOutput);
				}
				
				
			}
		}
	}
}

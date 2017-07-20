package com.gboxsw.arduino.mifarereader;

import java.util.*;

import com.gboxsw.acpmod.gep.GEPMessenger;
import com.gboxsw.acpmod.gep.GEPMessenger.*;

public class CardReader {

	/**
	 * Maximal length of received message.
	 */
	private static final int MAX_MESSAGE_LENGTH = 50;

	/**
	 * Default timeout of a command execution in milliseconds.
	 */
	private static final long DEFAULT_TIMEOUT = 500;

	/**
	 * Empty byte array.
	 */
	private static final byte[] EMPTY_COMMAND_DATA = new byte[0];

	/**
	 * Command codes for reader.
	 */
	private static final class CommandCode {
		/**
		 * Reset card.
		 */
		static final int RESET = 1;

		/**
		 * Set key.
		 */
		static final int SET_KEY = 2;

		/**
		 * Read block.
		 */
		static final int READ_BLOCK = 3;

		/**
		 * Write block.
		 */
		static final int WRITE_BLOCK = 4;

		/**
		 * Read sector trailer.
		 */
		static final int READ_SECTOR_TRAILER = 5;

		/**
		 * Write sector trailer.
		 */
		static final int WRITE_SECTOR_TRAILER = 6;
	}

	/**
	 * Codes of messages send by the reader.
	 */
	private static final class MessageCode {
		/**
		 * Response to successfully completed command.
		 */
		static final int COMMAND_OK = 1;

		/**
		 * Response to a failed command.
		 */
		static final int COMMAND_FAILED = 2;

		/**
		 * Notification that a new card is detected.
		 */
		static final int CARD_DETECTED = 3;

		/**
		 * Notification that the card was removed.
		 */
		static final int CARD_REMOVED = 4;
	}

	/**
	 * Card types.
	 */
	public enum CardType {
		ISO_14443_4(1), ISO_18092(2), MIFARE_MINI(3), MIFARE_1K(4), MIFARE_4K(5), MIFARE_UL(6), MIFARE_PLUS(7), TNP3XXX(
				8);

		/**
		 * Internal code of the card.
		 */
		private final int code;

		/**
		 * Constructs the card type.
		 * 
		 * @param code
		 *            the internal code.
		 */
		private CardType(int code) {
			this.code = code;
		}
	}

	/**
	 * The listener interface for receiving notifications about changes of card
	 * presence.
	 */
	public interface CardListener {
		/**
		 * Invoked when presence of card changed.
		 * 
		 * @param reader
		 *            the reader.
		 * @param cardPresent
		 *            true, if card is present, false, otherwise.
		 */
		public void cardChanged(CardReader reader, boolean cardPresent);
	}

	/**
	 * Sector trailer.
	 */
	public static class SectorTrailer {
		/**
		 * Key A
		 */
		public final byte[] keyA = new byte[6];

		/**
		 * Key B
		 */
		public final byte[] keyB = new byte[6];

		/**
		 * Access flags for block in the sector.
		 */
		public final byte[] accessFlags = new byte[4];

		/**
		 * General purpose byte (user data)
		 */
		public byte generalPurposeByte;
	}

	/**
	 * Messenger utilized to communicate with the reader.
	 */
	private final GEPMessenger messenger;

	/**
	 * Registered card listeners.
	 */
	private final List<CardListener> cardListeners = new ArrayList<>();

	/**
	 * Type of active card.
	 */
	private CardType cardType;

	/**
	 * Number of available blocks
	 */
	private int blockCount;

	/**
	 * Identifier of the card.
	 */
	private byte[] cardUID;

	/**
	 * Synchronization lock.
	 */
	private final Object lock = new Object();

	/**
	 * Timeout of command execution in milliseconds.
	 */
	private volatile long timeout = DEFAULT_TIMEOUT;

	/**
	 * Lock that controls execution of commands.
	 */
	private final Object commandLock = new Object();

	/**
	 * Counter for generating command tags.
	 */
	private int tagCounter = 1;

	/**
	 * Response data from reader as a result of executing command.
	 */
	private byte[] commandResponseData = null;

	/**
	 * Indicated whether response to sent command has been received.
	 */
	private boolean commandResponseReceived = false;

	/**
	 * Tag of executing command.
	 */
	private int commandTag = -1;

	/**
	 * Constructs the card reader.
	 * 
	 * @param socket
	 *            the socket providing access to device using the GEP protocol.
	 */
	public CardReader(FullDuplexStreamSocket socket) {
		messenger = new GEPMessenger(socket, 0, MAX_MESSAGE_LENGTH, new MessageListener() {
			public void onMessageReceived(int tag, byte[] message) {
				handleReceivedMessage(tag, message);
			}
		});
	}

	/**
	 * Starts the reader.
	 */
	public void start() {
		messenger.start(true);
	}

	/**
	 * Stops the reader.
	 */
	public void stop() {
		try {
			messenger.stop(true);
		} catch (InterruptedException ignore) {

		}
	}

	public void addCardListener(CardListener listener) {
		if (listener == null) {
			throw new NullPointerException("Listener cannot be null.");
		}

		synchronized (lock) {
			cardListeners.add(listener);
		}
	}

	public void removeCardListener(CardListener listener) {
		synchronized (lock) {
			cardListeners.remove(listener);
		}
	}

	public CardType getCardType() {
		synchronized (lock) {
			return cardType;
		}
	}

	public int getBlockCount() {
		synchronized (lock) {
			return blockCount;
		}
	}

	public byte[] getCardUID() {
		synchronized (lock) {
			if (cardUID != null) {
				return cardUID.clone();
			} else {
				return null;
			}
		}
	}

	public boolean setKeyA(byte[] key) {
		return setKey(key, true);
	}

	public boolean setKeyB(byte[] key) {
		return setKey(key, false);
	}

	private boolean setKey(byte[] key, boolean isAKey) {
		if ((key == null) || (key.length != 6)) {
			throw new IllegalArgumentException("Key must have the length 6.");
		}

		byte[] commandData = new byte[1 + 6];
		commandData[0] = (byte) (isAKey ? 1 : 2);
		System.arraycopy(key, 0, commandData, 1, 6);

		return sendCommand(CommandCode.SET_KEY, commandData, timeout) != null;
	}

	public boolean resetCard() {
		return sendCommand(CommandCode.RESET, EMPTY_COMMAND_DATA, timeout) != null;
	}

	public byte[] readBlock(int block) {
		if ((block < 0) || (block > 255)) {
			throw new IllegalArgumentException("Block must be between 0 and 255.");
		}

		byte[] commandData = new byte[1];
		commandData[0] = (byte) block;

		return sendCommand(CommandCode.READ_BLOCK, commandData, timeout);
	}

	public boolean writeBlock(int block, byte[] data) {
		if (data == null) {
			throw new NullPointerException("Data cannot be null.");
		}

		if ((block < 0) || (block > 255)) {
			throw new IllegalArgumentException("Block must be between 0 and 255.");
		}

		byte[] commandData = new byte[1 + data.length];
		commandData[0] = (byte) block;
		System.arraycopy(data, 0, commandData, 1, data.length);

		return sendCommand(CommandCode.WRITE_BLOCK, commandData, timeout) != null;
	}

	public SectorTrailer readSectorTrailer(int sector) {
		if ((sector < 0) || (sector > 255)) {
			throw new IllegalArgumentException("Sector must be between 0 and 255.");
		}

		byte[] commandData = new byte[1];
		commandData[0] = (byte) sector;

		byte[] response = sendCommand(CommandCode.READ_SECTOR_TRAILER, commandData, timeout);
		if ((response == null) || (response.length != 17)) {
			return null;
		}

		SectorTrailer result = new SectorTrailer();
		for (int i = 0; i < 4; i++) {
			result.accessFlags[i] = response[i];
		}

		System.arraycopy(response, 4, result.keyA, 0, 6);
		System.arraycopy(response, 10, result.keyB, 0, 6);
		result.generalPurposeByte = response[16];

		return result;
	}

	public boolean writeSectorTrailer(int sector, SectorTrailer sectorTrailer) {
		if (sectorTrailer == null) {
			throw new NullPointerException("Sector trailer cannot be null.");
		}

		if ((sector < 0) || (sector > 255)) {
			throw new IllegalArgumentException("Sector must be between 0 and 255.");
		}

		byte[] commandData = new byte[1 + 17];
		commandData[0] = (byte) sector;
		System.arraycopy(sectorTrailer.accessFlags, 0, commandData, 1, 4);
		System.arraycopy(sectorTrailer.keyA, 0, commandData, 5, 6);
		System.arraycopy(sectorTrailer.keyB, 0, commandData, 11, 6);
		commandData[17] = sectorTrailer.generalPurposeByte;

		return sendCommand(CommandCode.WRITE_SECTOR_TRAILER, commandData, timeout) != null;
	}

	/**
	 * Sends a command to execute by card reader.
	 * 
	 * @param commandCode
	 *            the code of command.
	 * @param commandData
	 *            the data.
	 * @param timeout
	 *            the timeout to complete command in milliseconds.
	 * @return response of command or null, if the execution of command failed.
	 */
	private byte[] sendCommand(int commandCode, byte[] commandData, long timeout) {
		// construct message
		byte[] message = new byte[commandData.length + 1];
		message[0] = (byte) commandCode;
		System.arraycopy(commandData, 0, message, 1, commandData.length);

		long startNanoTime = System.nanoTime();
		synchronized (commandLock) {
			// wait for "free" reader (no executing command)
			while (commandTag != -1) {
				try {
					long remainingMillis = computeRemainingTimeInMillis(startNanoTime, timeout);
					if (remainingMillis <= 0) {
						return null;
					}

					commandLock.wait(remainingMillis);
				} catch (InterruptedException e) {
					return null;
				}
			}

			// increment command tag
			tagCounter++;
			if (tagCounter > 10000) {
				tagCounter = 1;
			}

			// send command to reader
			try {
				commandResponseData = null;
				commandResponseReceived = false;
				commandTag = tagCounter;
				messenger.sendMessage(0, message, tagCounter);
			} catch (Exception e) {
				// clear execution
				commandTag = -1;
				commandLock.notifyAll();
				return null;
			}

			// wait for response
			while (!commandResponseReceived) {
				try {
					long remainingMillis = computeRemainingTimeInMillis(startNanoTime, timeout);
					if (remainingMillis <= 0) {
						break;
					}

					commandLock.wait(remainingMillis);
				} catch (InterruptedException e) {
					break;
				}
			}

			byte[] result = commandResponseData;
			commandResponseData = null;
			commandResponseReceived = false;
			commandTag = -1;

			return result;
		}
	}

	/**
	 * Handles a message received from reader.
	 * 
	 * @param tag
	 *            the tag of message.
	 * @param message
	 *            the message data.
	 */
	private void handleReceivedMessage(int tag, byte[] message) {
		if ((message == null) || (message.length == 0)) {
			return;
		}

		int messageCode = message[0] & 0xFF;

		if (messageCode == MessageCode.CARD_DETECTED) {
			handleCardDetected(message);
		} else if (messageCode == MessageCode.CARD_REMOVED) {
			handleCardRemoved();
		} else if ((messageCode == MessageCode.COMMAND_OK) || (messageCode == MessageCode.COMMAND_FAILED)) {
			synchronized (commandLock) {
				if (tag == commandTag) {
					commandResponseReceived = true;
					if (messageCode == MessageCode.COMMAND_OK) {
						commandResponseData = new byte[message.length - 1];
						System.arraycopy(message, 1, commandResponseData, 0, commandResponseData.length);
					}

					commandLock.notifyAll();
				}
			}
		}
	}

	/**
	 * Handles notification that a new card is detected.
	 * 
	 * @param message
	 *            the notification data.
	 */
	private void handleCardDetected(byte[] message) {
		if (message.length < 3) {
			return;
		}

		handleCardRemoved();

		List<CardListener> listenersToFire = null;
		synchronized (lock) {
			// decode card type
			cardType = null;
			int cardTypeCode = message[1] & 0xFF;
			for (CardType ct : CardType.values()) {
				if (ct.code == cardTypeCode) {
					cardType = ct;
					break;
				}
			}

			if (cardType == null) {
				return;
			}

			// decode block count
			blockCount = message[2] & 0xFF;

			// retrieve UID of the card
			cardUID = new byte[message.length - 3];
			for (int i = 0; i < cardUID.length; i++) {
				cardUID[i] = message[i + 3];
			}

			listenersToFire = new ArrayList<>(cardListeners);
		}

		if (listenersToFire != null) {
			for (CardListener cardListener : listenersToFire) {
				cardListener.cardChanged(this, true);
			}
		}
	}

	/**
	 * Handle notification that a card was removed.
	 */
	private void handleCardRemoved() {
		List<CardListener> listenersToFire = null;
		synchronized (lock) {
			if (cardType == null) {
				return;
			}

			cardType = null;
			cardUID = null;
			blockCount = 0;

			listenersToFire = new ArrayList<>(cardListeners);
		}

		if (listenersToFire != null) {
			for (CardListener cardListener : listenersToFire) {
				cardListener.cardChanged(this, false);
			}
		}
	}

	/**
	 * Computes remaining time in milliseconds to complete an operation.
	 * 
	 * @param startNanoTime
	 *            time when operation started as system nanoseconds.
	 * @param timeout
	 *            the timeout of operation in milliseconds.
	 * @return remaining time in milliseconds.
	 */
	private static long computeRemainingTimeInMillis(long startNanoTime, long timeout) {
		long elapsedTime = (System.nanoTime() - startNanoTime) / 1_000_000;
		return timeout - elapsedTime;
	}
}

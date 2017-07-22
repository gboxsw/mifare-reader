package com.gboxsw.arduino.mifarereader;

public class ArrayConverter {

	public static int[] convertToIntArray(byte[] array) {
		if (array == null) {
			return null;
		}

		int[] result = new int[array.length];
		for (int i = 0; i < array.length; i++) {
			result[i] = array[i] & 0xff;
		}

		return result;
	}

	public static byte[] convertToByteArray(int[] array) {
		if (array == null) {
			return null;
		}

		byte[] result = new byte[array.length];
		for (int i = 0; i < array.length; i++) {
			result[i] = (byte) array[i];
		}

		return result;
	}

	public static String convertToHexString(int[] array, String separator) {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < array.length; i++) {
			String hexByte = Integer.toHexString(array[i]).toUpperCase();
			if (hexByte.length() == 1) {
				hexByte = "0" + hexByte;
			}

			if (i != 0) {
				sb.append(separator);
			}

			sb.append(hexByte);
		}

		return sb.toString();
	}

	public static String convertToHexString(byte[] array, String separator) {
		return convertToHexString(convertToIntArray(array), separator);
	}

}

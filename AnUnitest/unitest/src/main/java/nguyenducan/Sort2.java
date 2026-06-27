package nguyenducan;

public class Sort2 {
    public static int number1;
    public static int number2;

    public static void sortDesc() {
        if (number1 < number2) {
            int temp = number1;
            number1 = number2;
            number2 = temp;
        }
    }
}
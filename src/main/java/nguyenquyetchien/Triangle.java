package nguyenquyetchien;

public class Triangle {
public int number1;
public int number2;
public int number3;
public int maxLength() {
if (number1 >= number2)
if (number1 > number3)
return number1;
else
return number3;
if (number2 > number3)
return number2;
else
return number3;
}
} 

class MeasureArrayHandler {

  public:
    short elementsAdded = 0;
    int* measureArray;
    int arraySize;

    float lastMean = 0;

    
  
    MeasureArrayHandler(int arraySize){
      this -> arraySize = arraySize;
      measureArray = new int[arraySize];
    }

    MeasureArrayHandler(int arraySize, int lastMean){
      this -> lastMean = lastMean;
      this -> arraySize = arraySize;
      measureArray = new int[arraySize];
    }

    int getAverage(){
      if(elementsAdded == 0)
        return lastMean;
        
      int sum = 0;
      for(int i=0; i<arraySize; i++)
        sum += measureArray[i];
      return ((int) sum / elementsAdded);
    }

    void addElement(int newValue){
      for(int i=0; i<arraySize - 1; i++){
        measureArray[i] = measureArray[i + 1];
      }
      measureArray[arraySize - 1] = newValue;

      if(elementsAdded < arraySize)
        elementsAdded++;
    }

    void printArray(){
      Serial.print("[");
        Serial.print(F(" "));
      for(int i=arraySize - elementsAdded; i < arraySize; i++){
        Serial.print(measureArray[i]);
        Serial.print(F(" "));
      }
      Serial.println(F("]"));
    }

    void clearMeasures(){
      lastMean = getAverage();
      for(int i = 0; i < arraySize; i++)
        measureArray[i] = 0;
      elementsAdded = 0;
    }
};

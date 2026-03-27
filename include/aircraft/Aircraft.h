#pragma once

namespace aircraft {

  /**
   * @brief Aircraft client component
   */
  class Aircraft {
  public:
    /**
     * @brief Creates a new aircraft client
     */
    Aircraft();

    /**
     * @brief Initialize the aircraft client
     */
    void initialize();

    int token = 0;
  };

}  // namespace aircraft

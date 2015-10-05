<?php
/**
 * Extension of QuizQuestion.
 */
class SimulatorQuestion extends QuizQuestion {
  /**
   * Implementation of saveNodeProperties
   */
  public function saveNodeProperties($is_new = FALSE) {
    if (!isset($this->node->feedback)) {
      $this->node->feedback = '';
    }
    if ($is_new || $this->node->revision == 1) {
      $id = db_insert('quiz_simulator_question_node_properties')
        ->fields(array(
          'nid' => $this->node->nid,
          'vid' => $this->node->vid,
          'metric' => $this->node->metric,
          'threshold' => $this->node->threshold,
          'required' => $this->node->required,
        ))
        ->execute();
    }
    else {
      db_update('quiz_simulator_question_node_properties')
        ->fields(array(
          'metric' => $this->node->metric,
          'threshold' => $this->node->threshold,
          'required' => $this->node->required,
        ))
        ->condition('nid', $this->node->nid)
        ->condition('vid', $this->node->vid)
        ->execute();
    }
  }

  /**
   * Implementation of validateNode
   */
  public function validateNode(array &$form) { }

  /**
   * Implementation of delete
   */
  public function delete($only_this_version = FALSE) {
    if ($only_this_version) {
      db_delete('quiz_simulator_question_user_answers')
        ->condition('question_nid', $this->node->nid)
        ->condition('question_vid', $this->node->vid)
        ->execute();
      db_delete('quiz_simulator_question_node_properties')
        ->condition('nid', $this->node->nid)
        ->condition('vid', $this->node->vid)
        ->execute();
    }
    else {
      db_delete('quiz_simulator_question_node_properties')
        ->condition('nid', $this->node->nid)
        ->execute();
      db_delete('quiz_simulator_question_user_answers')
        ->condition('question_nid', $this->node->nid)
        ->execute();
    }
    parent::delete($only_this_version);
  }

  /**
   * Implementation of getNodeProperties
   */
  public function getNodeProperties() {
    if (isset($this->nodeProperties)) {
      return $this->nodeProperties;
    }
    $props = parent::getNodeProperties();

    $res_a = db_query('SELECT metric, threshold, required FROM {quiz_simulator_question_node_properties}
      WHERE nid = :nid AND vid = :vid', array(':nid' => $this->node->nid, ':vid' => $this->node->vid))->fetchAssoc();

    if (is_array($res_a)) {
      $props = array_merge($props, $res_a);
    }

    $this->nodeProperties = $props;
    return $props;
  }

  /**
   * Implementation of getNodeView
   */
  public function getNodeView() {
    $content = parent::getNodeView();
    /*
    if ($this->viewCanRevealCorrect()) {
      $content['answers'] = array(
        '#type' => 'item',
        '#title' => t('Rubric'),
        '#markup' => '<div class="quiz-solution">' . check_markup($this->node->rubric, $this->getFormat()) . '</div>',
        '#weight' => 1,
      );
    }
    else {
      $content['answers'] = array(
        '#markup' => '<div class="quiz-answer-hidden">Answer hidden</div>',
        '#weight' => 1,
      );
    }
    */
    return $content;
  }

  /**
   * Implementation of getAnweringForm
   */
  public function getAnsweringForm(array $form_state = NULL, $rid) {
    $response = new SimulatorResponse($rid, $this->node);
    return;
  }

  /**
   * Implementation of getCreationForm
   */
  public function getCreationForm(array &$form_state = NULL) {
    $form['metric'] = array(
      '#type' => 'select',
      '#title' => t('Metric'),
      '#description' => t('Specify the criteria for grading the response.'),
      '#default_value' => isset($this->node->metric) ? $this->node->metric : '',
      '#required' => FALSE,
      '#options' => simulator_question_metric_options(),
    );
    $form['threshold'] = array(
      '#type' => 'textfield',
      '#title' => t('Threshold'),
      '#description' => t('Specify the threshold value used for evaluation within the simulator.'),
      '#default_value' => isset($this->node->threshold) ? $this->node->threshold : '',
      '#required' => FALSE,
    );
    $form['required'] = array(
      '#type' => 'checkbox',
      '#title' => t('Required'),
      '#description' => t('Failure to pass this question will fail the lesson.'),
      '#default_value' => isset($this->node->required) ? $this->node->required : '',
      '#required' => FALSE,
    );
    return $form;
  }

  /**
   * Implementation of getMaximumScore
   */
  public function getMaximumScore() {
    return variable_get('simulator_question_default_max_score', 10);
  }

  public function getSimulationConfig() {
    $config = array();
    $config['metric'] = $this->node->metric;
    $config['threshold'] = $this->node->threshold;
    // Probably need another parameter for how to evaluate threshold.
    return $config;
  }

}

/**
 * Extension of QuizQuestionResponse
 */
class SimulatorResponse extends QuizQuestionResponse {

  /**
   * ID of the answer.
   */
  protected $answer_id = 0;
  protected $is_correct = FALSE;
  protected $evaluated = FALSE;
  protected $answer = NULL;

  /**
   * Constructor
   */
  public function __construct($result_id, stdClass $question_node, $answer = NULL) {
    parent::__construct($result_id, $question_node, $answer);

    if (!isset($answer)) {
      // Question has been answered allready. We fetch the answer data from the database.
      $r = db_query('SELECT answer_id, answer, is_evaluated, score, question_vid, question_nid, result_id, answer_feedback, answer_feedback_format
        FROM {quiz_simulator_question_user_answers}
        WHERE question_nid = :qnid AND question_vid = :qvid AND result_id = :rid', array(':qnid' => $question_node->nid, ':qvid' => $question_node->vid, ':rid' => $result_id))->fetch();

      if (!empty($r)) {
        $this->answer = $r->answer;
        $this->score = $r->score;
        $this->evaluated = $r->is_evaluated;
        $this->answer_id = $r->answer_id;
        $this->answer_feedback = $r->answer_feedback;
        $this->answer_feedback_format = $r->answer_feedback_format;
      }
    }
    else {
      $this->answer = $answer;
      $this->evaluated = FALSE;
    }
  }

  /**
   * Implementation of isValid
   */
  public function isValid() {
    return TRUE;
  }

  /**
   * Implementation of save
   */
  public function save() {
    $this->answer_id = db_insert('quiz_simulator_question_user_answers')
      ->fields(array(
        'answer' => 0,
        'question_nid' => $this->question->nid,
        'question_vid' => $this->question->vid,
        'result_id' => $this->rid,
      ))
      ->execute();
  }

  /**
   * Implementation of delete
   */
  public function delete() {
    db_delete('quiz_simulator_question_user_answers')
      ->condition('question_nid', $this->question->nid)
      ->condition('question_vid', $this->question->vid)
      ->condition('result_id', $this->rid)
      ->execute();
  }

  /**
   * Implementation of score
   */
  public function score() {
    return (int) db_query('SELECT score FROM {quiz_simulator_question_user_answers}
      WHERE result_id = :result_id AND question_vid = :question_vid', array(':result_id' => $this->rid, ':question_vid' => $this->question->vid))->fetchField();
  }

  /**
   * Implementation of getResponse
   */
  public function getResponse() {
    return $this->answer;
  }

  /**
   * Implementation of getReportFormResponse
   */
  public function getReportFormResponse($showpoints = TRUE, $showfeedback = TRUE, $allow_scoring = FALSE) {
    $form = array();
    $form['#theme'] = 'simulator_question_response_form';
    if ($this->question && !empty($this->question->answers)) {
      $answer = (object) current($this->question->answers);
    }
    else {
      return $form;
    }
    //$form['answer'] = array('#markup' => check_markup($answer->answer));
    if ($answer->is_evaluated == 1) {
      // Show feedback, if any.
      if ($showfeedback && !empty($answer->feedback)) {
        //$feedback = check_markup($answer->feedback);
      }
    }
    else {
      $feedback = t('This answer has not yet been scored.') .
        '<br/>' .
        t('Until the answer is scored, the total score will not be correct.');
    }
    $form['metric'] = array(
      '#title' => t('Metric evaluated'),
      '#title_display' => 'invisible',
      '#type' => 'item',
      '#markup' => t('Metric') . ': <span>' . $this->question->metric . '</span>',
    );
    $form['threshold'] = array(
      '#title' => t('Threshold used for evaluation'),
      '#title_display' => 'invisible',
      '#type' => 'item',
      '#markup' => t('Threshold used for evaluation') . ': <span>' . $this->question->threshold . '</span>',
    );
/*
    $form['required'] = array(
      '#title' => t('Is this question required to pass'),
      '#type' => 'item',
      '#markup' => $this->question->required,
    );
*/

    if ($allow_scoring) {

    }
    if (!$allow_scoring && !empty($this->answer_feedback)) {
      $form['answer_feedback'] = array(
        '#title' => t('Feedback'),
        '#type' => 'item',
        '#markup' => '<span class="quiz_answer_feedback">' . check_markup($this->answer_feedback, $this->answer_feedback_format) . '</span>',
      );
    }
    if (!empty($feedback)) {
      $form['feedback'] = array('#markup' => '<span class="quiz_answer_feedback">' . $feedback . '</span>');
    }
    return $form;
  }

  /**
   * Implementation of getReportFormScore
   */
  public function getReportFormScore($showfeedback = TRUE, $showpoints = TRUE, $allow_scoring = FALSE) {
    // The score will be shown as a questionmark if the question haven't been scored already
    $score = ($this->isEvaluated()) ? $this->getScore() : 0;
    return array('#markup' => $score);
  }

  public function getReportFormAnswerFeedback($showpoints = TRUE, $showfeedback = TRUE, $allow_scoring = FALSE) {
/*
    if (quiz_access_to_score() && $allow_scoring) {
      return array(
        '#title' => t('Feedback'),
        '#type' => 'textarea',
        '#rows' => 2,
        '#default_value' => isset($this->answer_feedback) ? check_markup($this->answer_feedback, $this->answer_feedback_format) : '',
        '#format' => isset($this->answer_feedback_format) ? $this->answer_feedback_format : NULL,
        '#attributes' => array('class' => array('quiz-report-score')),
      );
    }
*/
    return FALSE;
  }


  /**
   * Implementation of getReportFormSubmit
   */
  public function getReportFormSubmit($showfeedback = TRUE, $showpoints = TRUE, $allow_scoring = FALSE) {
    return $allow_scoring ? 'simulator_question_report_submit' : FALSE;
  }

  /**
   * Implementation of getReportFormValidate
   */
  public function getReportFormValidate($showfeedback = TRUE, $showpoints = TRUE, $allow_scoring = FALSE) {
    //return $allow_scoring ? 'simulator_question_report_validate' : FALSE;
  }

  /**
   * Check to see if the answer is marked as correct.
   */
  function getScore($weight_adjusted = TRUE) {
    if ($this->is_skipped) {
      return 0;
    }
    if (!isset($this->score)) {
      $this->score = $this->score();
    }
    if (isset($this->question->score_weight) && $weight_adjusted) {
      return round($this->score * $this->question->score_weight);
    }
    return $this->score;
  }
}
